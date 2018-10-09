/*
 * romfs.c
 * Implements romfs devoptab using ustar fs
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/dirent.h>
#include <sys/iosupport.h>
#include <sys/param.h>
#include <stdint.h>
#include <unistd.h>
#include <coreinit/memory.h>

//!----------------------------------------------------------------------
//! Filesystem tree functions
//!----------------------------------------------------------------------

typedef enum {
	NODE_DIR,
	NODE_FILE,
} node_type_t;

typedef struct _node node_t;
struct _node {
	char *		name;
	unsigned	inode;
	unsigned	size;
	node_type_t	type;
	time_t		mtime;
	union {
		uint8_t	*cont;	// NODE_FILE
		node_t	*ent;	// NODE_DIR
	};
	node_t *	next;
	node_t *	up;
};

static node_t fs_root = {
	.inode = 0,
	.name = "romfs:",
	.type = NODE_DIR,
	.ent = NULL,
	.next = NULL,
	.up = NULL,
};
static node_t *fs_cwd = &fs_root;

static unsigned n_nodes = 0;

// Create a node and all its parent directories
static void node_createfilepath (const char *_path, time_t mtime, int isdir, unsigned size, uint8_t *content) {
	node_t *n = &fs_root;
	char *path = strdup(_path), *path_s;
	char *cur = strtok_r(path, "/", &path_s);	 
	while (cur)  {
		// check if there's another path next, and get it if it exists
		char *next = strtok_r(NULL, "/", &path_s);
		if (!strcmp(cur, ".")) {
			n = n;
		} else if (!strcmp(cur, "..")) {
			if (n->up) n = n->up;
		} else {
			// check if the requested node already exists
			node_t *c = NULL;
			for (c = n->ent; c && strcmp(c->name, cur); c = c->next);
			// if it doesn't, create it
			if (!c) {
				c = malloc(sizeof(node_t));
				c->name = strdup(cur);
				c->inode = ++n_nodes;
				c->mtime = mtime;
				if (next) { // there's another path next, this is a dir
					c->type = NODE_DIR;
					c->ent = NULL;
				} else { // there's no other path next, this is the final node
					c->size = size;
					if (isdir) {
						c->type = NODE_DIR;
						c->ent = NULL;	
					} else {
						c->type = NODE_FILE;
						c->cont = content;
					}
				}
				// add the node to tree
				c->up = n;
				c->next = n->ent;
				n->ent = c;
			}
			// child node is now parent node
			n = c;
		}
		cur = next;
	}
	free(path);
}

// Frees the tree of nodes
static void node_destroytree (node_t *n, int recursion) {
	// this is the easier way, to prevent freeing the wrong part of tree first
	static node_t **l_free = NULL;
	static unsigned l_freecnt = 0;
	if (!l_free)
		l_free = malloc(n_nodes * sizeof(node_t *));
	if (!n)
		n = &fs_root;
	// it's better to leave some stuff in the stack than risking a stack overflow,
	// so if there are more than 50 levels of folders we stop here.
	if (recursion > 50)
		return;
	// trasverse the node tree
	for (node_t *c = n->ent; c; c = c->next) {
		if (c->type == NODE_DIR)
			node_destroytree(c, recursion + 1);
		l_free[l_freecnt++] = c;
	}
	// if we're a recursive call, return now
	if (recursion)
		return;
	// we're freeing the fs_root node; shall the free() start
	for (int i = 0; i < l_freecnt; i++)
		free(l_free[i]);
	// finish freeing stuff
	free(l_free);
	l_freecnt = 0;
	l_free = NULL;
	fs_root.ent = NULL;
}

// Parse path string and return the corresponding node
static node_t *node_get (const char *_path) {
	char* col_pos = strchr(_path, ':');
	if (col_pos) _path = col_pos + 1;
	node_t *n = (_path[0] == '/') ? &fs_root : fs_cwd;
	char *path = strdup(_path), *path_s;
	char *cur = strtok_r(path, "/", &path_s);
	while (cur) {
		if (!n || (n->type != NODE_DIR)) {
			n = NULL;
			break;
		}
		if (!strcmp(cur, ".")) {
			n = n;
		} else if (!strcmp(cur, "..")) {
			if (n->up) n = n->up;
		} else {
			for (n = n->ent; n && strcmp(n->name, cur); n = n->next);
		}
		cur = strtok_r(NULL, "/", &path_s);
	};
	free(path);
	return n;
}

//!----------------------------------------------------------------------
//! TAR functions
//!----------------------------------------------------------------------

typedef struct __attribute__((packed)) {
	char fname[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char fsize[12];
	char mtime[12];
	char chksum[8];
	char typeflag[1];
	char link[100];
	char tar_magic[6];
	char tar_version[2];
	char username[32];
	char groupname[32];
	char dev_maj[8];
	char dev_min[8];
	char prefix[155];
	char pad[12];
} tar_header_t;

static unsigned oct2bin(uint8_t *c, int size) {
	unsigned n = 0;
	while (size-- > 0)
		n = (n * 8) + (*(c++) - '0');
	return n;
}

// Create tree entries for tar's files and folders
static void tar_create_entries(uint8_t *ptr, uint8_t *end) {
	tar_header_t *hdr = (tar_header_t *)ptr;
	tar_header_t *ehdr = (tar_header_t *)end;
	while ((hdr < ehdr) && !memcmp(hdr->tar_magic, "ustar", 5)) {
		unsigned fsize = oct2bin(hdr->fsize, 11);
		unsigned mtime = oct2bin(hdr->mtime, 11);
		int isdir = hdr->typeflag[0] == '5';
		int isfile = hdr->typeflag[0] == '0';
		if (isfile || isdir)
			node_createfilepath(hdr->fname, mtime, isdir, fsize, (uint8_t *)(hdr + 1));
		hdr += ((fsize + 511) / 512) + 1;
	}
}

//!----------------------------------------------------------------------
//! Devoptab functions
//!----------------------------------------------------------------------

typedef struct {
	node_t *fsnode;
	off_t pos;
} romfs_file_t;

typedef struct {
	node_t *fsdir;
	node_t *ent;
	int idx;
} romfs_dir_t;

#define ROMFS_DIR_MODE	(S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH)
#define ROMFS_FILE_MODE	(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH)

static int romfs_open (struct _reent *r, void *fileStruct, const char *ipath, int flags, int mode) {
	romfs_file_t *fobj = (romfs_file_t *)fileStruct;
	fobj->fsnode = node_get(ipath);
	if (!fobj->fsnode) {
		if(flags & O_CREAT)
			r->_errno = EROFS;
		else
			r->_errno = ENOENT;
		return -1;
	} else if ((flags & O_CREAT) && (flags & O_EXCL)) {
		r->_errno = EEXIST;
		return -1;
	}
	if (fobj->fsnode->type != NODE_FILE) {
		r->_errno = EINVAL;
		return -1;
	}
	fobj->pos = 0;
	return 0;
}


static int romfs_close(struct _reent *r, void *fd) {
	return 0;
}

static ssize_t romfs_read(struct _reent *r, void *fd, char *ptr, size_t len) {
	romfs_file_t *fobj = (romfs_file_t *)fd;	
	size_t end_pos = fobj->pos + len;
	if(fobj->pos >= fobj->fsnode->size)
		return 0;
	if((fobj->pos + len) > fobj->fsnode->size)
		len = fobj->fsnode->size - fobj->pos;
	OSBlockMove(ptr, fobj->fsnode->cont + fobj->pos, len, FALSE);
	fobj->pos += len;
	return len;
}

static off_t romfs_seek(struct _reent *r, void *fd, off_t pos, int dir) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	off_t start;
	switch (dir) {
		case SEEK_SET:
			start = 0;
			break;
		case SEEK_CUR:
			start = fobj->pos;
			break;
		case SEEK_END:
			start = fobj->fsnode->size;
			break;
		default:
			r->_errno = EINVAL;
			return -1;
    }
	if(pos < 0) {
		if(start + pos < 0) {
			r->_errno = EINVAL;
			return -1;
		}
	} else if(INT32_MAX - pos < start) {
		r->_errno = EOVERFLOW;
		return -1;
	}
    fobj->pos = start + pos;
    return fobj->pos;
}

static int romfs_fstat(struct _reent *r, void *fd, struct stat *st) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	memset(st, 0, sizeof(struct stat));
	st->st_size  = fobj->fsnode->size;
	st->st_atime = st->st_mtime = st->st_ctime = fobj->fsnode->mtime;
	st->st_ino = fobj->fsnode->inode;
	st->st_mode  = ROMFS_FILE_MODE;
	st->st_nlink = 1;
	st->st_blksize = 512;
	st->st_blocks  = (st->st_blksize + 511) / 512;
	return 0;
}

static int romfs_stat(struct _reent *r, const char *ipath, struct stat *st) {
	node_t *fsnode = node_get(ipath);
	if (!fsnode) {
		r->_errno = ENOENT;
		return -1;
	}
	memset(st, 0, sizeof(struct stat));
	st->st_size  = fsnode->size;
	st->st_atime = st->st_mtime = st->st_ctime = fsnode->mtime;
	st->st_ino = fsnode->inode;
	st->st_mode  = (fsnode->type == NODE_FILE) ? ROMFS_FILE_MODE : ROMFS_DIR_MODE;
	st->st_nlink = 1;
	st->st_blksize = 512;
	st->st_blocks  = (st->st_blksize + 511) / 512;
	return 0;
}

static int romfs_chdir(struct _reent *r, const char *ipath) {
	node_t *fsdir = node_get(ipath);
	if (!fsdir) {
		r->_errno = ENOENT;
		return -1;
	}
	fs_cwd = fsdir;
	return 0;
}

static DIR_ITER* romfs_diropen(struct _reent *r, DIR_ITER *dirState, const char *ipath) {
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);
	dirobj->fsdir = node_get(ipath);
	if (!dirobj->fsdir) {
		r->_errno = ENOENT;
		return NULL;
	}
	if (dirobj->fsdir->type != NODE_DIR) {
		r->_errno = EINVAL;
		return NULL;
	}
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;
	return dirState;
}

static int romfs_dirreset(struct _reent *r, DIR_ITER *dirState) {
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;
	return 0;
}

static int romfs_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat) {
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);

	if (dirobj->idx == 0) { // .
		memset(filestat, 0, sizeof(*filestat));
		filestat->st_ino  = dirobj->fsdir->inode;
		filestat->st_mode = ROMFS_DIR_MODE;
		strcpy(filename, ".");
		dirobj->idx = 1;
		return 0;
	} else if (dirobj->idx == 1) { // ..
		node_t* updir = dirobj->fsdir->up;
		if (!updir) updir = &fs_root;
		memset(filestat, 0, sizeof(*filestat));
		filestat->st_ino = updir->inode;
		filestat->st_mode = ROMFS_DIR_MODE;
		strcpy(filename, "..");
		dirobj->idx = 2;
		return 0;
	}
	
	if (!dirobj->ent) {
		r->_errno = ENOENT;
		return -1;
	}

	// set data
	memset(filestat, 0, sizeof(*filestat));
	filestat->st_ino = dirobj->ent->inode;
	filestat->st_mode = (dirobj->ent->type == NODE_FILE) ? ROMFS_FILE_MODE : ROMFS_DIR_MODE;
	strcpy(filename, dirobj->ent->name);

	// go to next entry
	dirobj->ent = dirobj->ent->next;

	return 0;
}

static int romfs_dirclose(struct _reent *r, DIR_ITER *dirState) {
	return 0;
}

static devoptab_t romfs_devoptab = {
	.name         = "romfs",
	.structSize   = sizeof(romfs_file_t),
	.open_r       = romfs_open,
	.close_r      = romfs_close,
	.read_r       = romfs_read,
	.seek_r       = romfs_seek,
	.fstat_r      = romfs_fstat,
	.stat_r       = romfs_stat,
	.chdir_r      = romfs_chdir,
	.dirStateSize = sizeof(romfs_dir_t),
	.diropen_r    = romfs_diropen,
	.dirreset_r   = romfs_dirreset,
	.dirnext_r    = romfs_dirnext,
	.dirclose_r   = romfs_dirclose,
	.deviceData   = NULL,
};

//!----------------------------------------------------------------------
//! Library functions
//!----------------------------------------------------------------------

extern char _binary_romfs_tar_start[] __attribute__((weak));
extern char _binary_romfs_tar_end[] __attribute__((weak));

static int romfs_initialised = 0;

int romfsInit(void) {
	if (romfs_initialised)
		return 0;
	if (!_binary_romfs_tar_start || !_binary_romfs_tar_end)
		return -2;
	tar_create_entries(_binary_romfs_tar_start, _binary_romfs_tar_end);
	if (AddDevice(&romfs_devoptab) == -1) {
		node_destroytree(NULL, 0);
		return -1;
	}
	romfs_initialised = 1;
	return 0;
}

int romfsExit(void) {
	if (!romfs_initialised)
		return -1;
	RemoveDevice("romfs:");
	node_destroytree(NULL, 0);
	romfs_initialised = 0;
	return 0;
}