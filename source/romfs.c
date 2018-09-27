#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/dirent.h>
#include <sys/iosupport.h>
#include <sys/param.h>
#include <unistd.h>
#include <zip.h>

typedef struct _node node_t;
struct _node {
	char *name;
	int inode;
	enum {
		NODE_DIR,
		NODE_FILE,
	} type;
	union {
		unsigned izip;
		node_t *ent;
	};
	node_t *next;
	node_t *up;
};

typedef struct {
	node_t *fsnode;
	zip_file_t *zf;
} romfs_file_t;

typedef struct {
	node_t *fsdir;
	node_t *ent;
	int idx;
} romfs_dir_t;

#define ROMFS_DIR_MODE	(S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH)
#define ROMFS_FILE_MODE	(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH)

node_t root = {
	.inode = 0,
	.name = "romfs:",
	.type = NODE_DIR,
	.ent = NULL,
	.next = NULL,
	.up = NULL,
};

/* TODO: find some better way to embed the zip */
extern char _binary_romfs_zip_start[];
extern char _binary_romfs_zip_size[];

static int n_nodes = 0;
static node_t *romfs_cwd = &root;
static int romfs_initialised = 0;

static zip_source_t *romfs_zip_src;
static zip_t *romfs_zip;

/* node_createfilepath
 * Create a node and all its parent directories */
static void node_createfilepath (const char *_path, unsigned z_index) {
	int len = strlen(_path);
	if (len >= NAME_MAX)
		return;
	int isdir = _path[len-1] == '/';
	node_t *n = &root;
	char *path = strdup(_path), *path_s;
	char *cur = strtok_r(path, "/", &path_s);	 
	while (cur)  {
		// check if there's another path next, and get it if it exists
		char *next = strtok_r(NULL, "/", &path_s);
		// check if the requested node already exists
		node_t *c = NULL;
		for (c = n->ent; c && strcmp(c->name, cur); c = c->next);
		// if it doesn't, create it
		if (!c) {
			c = malloc(sizeof(node_t));
			c->name = strdup(cur);
			c->inode = ++n_nodes;
			if (next || isdir) { // there's another path next, this is a dir
				c->type = NODE_DIR;
				c->ent = NULL;
			} else { // there's no other path next, this is the file
				c->type = NODE_FILE;
				c->izip = z_index;
			}
			// add the node to tree
			c->up = n;
			c->next = n->ent;
			n->ent = c;
		}

		// child node is now parent node
		n = c;
		cur = next;
	}
	free(path);
}

/* Frees the tree of nodes
 * Although limited to only 50 recursions,
 * it's a recursive function so make sure there's enough
 * stack. */
static void node_destroytree (node_t *n, int recursion) {
	// this is the easier way, to prevent freeing the wrong part of tree first
	static node_t **l_free = NULL;
	static unsigned l_freecnt = 0;
	if (!l_free)
		l_free = malloc(n_nodes * sizeof(node_t *));
	if (!n)
		n = &root;
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
	// we're freeing the root node; shall the free() start
	for (int i = 0; i < l_freecnt; i++)
		free(l_free[i]);
	// finish freeing stuff
	free(l_free);
	l_freecnt = 0;
	l_free = NULL;
	root.ent = NULL;
}

/* Does much of the path parsing work; it will
 *  - remove "romfs:" when present
 *  - parse absolute/relative paths and add cwd
 *  - handle "." and ".."
 *  - get the node from the tree */
static node_t *node_get (const char *_path) {
	char* col_pos = strchr(_path, ':');
	if (col_pos) _path = col_pos + 1;
	node_t *n = (_path[0] == '/') ? &root : romfs_cwd;
	char *path = strdup(_path), *path_s;
	char *cur = strtok_r(path, "/", &path_s);
	while (cur) {
		if (!n || (n->type != NODE_DIR)) {
			n = NULL;
			break;
		}
		if (!strcmp(cur, ".")) {
			// nothing to do
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

static int romfs_open (struct _reent *r, void *fileStruct, const char *ipath, int flags, int mode) {
	romfs_file_t *fobj = (romfs_file_t *)fileStruct;
	fobj->fsnode = node_get(ipath);
	if (!fobj->fsnode) {
		r->_errno = ENOENT;
		return -1;
	}
	if (fobj->fsnode->type != NODE_DIR) {
		r->_errno = EINVAL;
		return -1;
	}
	fobj->zf = zip_fopen_index(romfs_zip, fobj->fsnode->izip, 0);
	if (!fobj->zf) {
		if(flags & O_CREAT)
			r->_errno = EROFS;
		else
			r->_errno = ENOENT;
		return -1;
	} else if ((flags & O_CREAT) && (flags & O_EXCL)) {
		r->_errno = EEXIST;
		return -1;
	}
	
	return 0;
}


static int romfs_close(struct _reent *r, void *fd) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	if (zip_fclose(fobj->zf)) {
		r->_errno = EINVAL;
		return -1;
	}
	return 0;
}

static ssize_t romfs_read(struct _reent *r, void *fd, char *ptr, size_t len) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	ssize_t n = zip_fread(fobj->zf, ptr, len);
	if (n < 0) {
		r->_errno = EIO;
		return -1;
	}
	return n;
}

static off_t romfs_seek(struct _reent *r, void *fd, off_t pos, int dir) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	if(zip_fseek(fobj->zf, pos, dir) == -1) {
		r->_errno = EINVAL;
		return -1;
	}
	return zip_ftell(fobj->zf);
}

static int romfs_fstat(struct _reent *r, void *fd, struct stat *st) {
	romfs_file_t *fobj = (romfs_file_t *)fd;
	memset(st, 0, sizeof(struct stat));
	
	zip_stat_t zst;
	if (zip_stat_index(romfs_zip, fobj->fsnode->izip, 0, &zst) == -1) {
		r->_errno = EIO;
		return -1;
	}
	
	if(zst.valid & ZIP_STAT_SIZE) st->st_size  = zst.size;
	if(zst.valid & ZIP_STAT_MTIME) st->st_atime = st->st_mtime = st->st_ctime = zst.mtime;
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
	
	zip_stat_t zst;
	if (zip_stat_index(romfs_zip, fsnode->izip, 0, &zst) == -1) {
		r->_errno = EIO;
		return -1;
	}
	
	if(zst.valid & ZIP_STAT_SIZE) st->st_size  = zst.size;
	if(zst.valid & ZIP_STAT_MTIME) st->st_atime = st->st_mtime = st->st_ctime = zst.mtime;
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
	romfs_cwd = fsdir;
	return 0;
}

static DIR_ITER* romfs_diropen(struct _reent *r, DIR_ITER *dirState, const char *ipath) {
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);
	dirobj->fsdir = node_get(ipath);
	if (!dirobj->fsdir) {
		r->_errno = ENOENT;
		return NULL;
	}
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;
	return dirState;
}

int romfs_dirreset(struct _reent *r, DIR_ITER *dirState) {
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;
	return 0;
}

int romfs_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat) {
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
		if (!updir) updir = &root;
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

uint32_t romfsInit() {
	zip_error_t error;

	if (romfs_initialised)
		return 0;

	zip_error_init(&error);

	// try to create zip source
	if ((romfs_zip_src = zip_source_buffer_create(_binary_romfs_zip_start, (int)_binary_romfs_zip_size, 1, &error)) == NULL) {
		zip_error_fini(&error);
		return 1;
	}

	// try to open zip archive from source
	if ((romfs_zip = zip_open_from_source(romfs_zip_src, 0, &error)) == NULL) {
		zip_source_free(romfs_zip_src);
		zip_error_fini(&error);
		return 1;
	}
	zip_error_fini(&error);

	// create the fs nodes from files in the zip
	zip_stat_t sb;
	for (int i = 0; i < zip_get_num_entries(romfs_zip, 0); i++)
		if (zip_stat_index(romfs_zip, i, 0, &sb) == 0)
			node_createfilepath(sb.name, i);

	// add the devoptab
	if (AddDevice(&romfs_devoptab) == -1) {
		zip_close(romfs_zip);
		zip_source_free(romfs_zip_src);
		return 1;
	}

	romfs_initialised = 1;

	return 0;
}

uint32_t romfsExit() {
	if (!romfs_initialised)
		return 1;

	// remove the devoptab
	RemoveDevice("romfs:");

	// destroy the tree of nodes
	node_destroytree(NULL, 0);

	// close archive
	if (zip_close(romfs_zip) < 0)
		return 1;

	// free zip source
	zip_source_free(romfs_zip_src);

	romfs_initialised = 0;

	return 0;
}