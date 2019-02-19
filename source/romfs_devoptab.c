/*
 * romfs.c
 * romfs devoptab implementation
 * Copyright (C) 2019 rw-r-r-0644
 */

#ifndef _ROMFS_DEVOPTAB_C
#define _ROMFS_DEVOPTAB_C

/* file informations structure */
typedef struct
{
	node_t *fsnode;
	off_t pos;
} romfs_file_t;

/* directory informations structure */
typedef struct
{
	node_t *fsdir;
	node_t *ent;
	int32_t idx;
} romfs_dir_t;

/* filesystem mode for all files or directories */
#define ROMFS_DIR_MODE	(S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH)
#define ROMFS_FILE_MODE	(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH)

static int romfs_open (struct _reent *r, void *fileStruct, const char *ipath, int flags, int mode)
{
	romfs_file_t *fobj = (romfs_file_t *)fileStruct;

	/* attempt to find the correct node for the path */
	fobj->fsnode = node_get(ipath);

	/* the path doesn't exist */
	if (!fobj->fsnode)
	{
		if(flags & O_CREAT)
			r->_errno = EROFS;
		else
			r->_errno = ENOENT;
		return -1;
	}
	/* the file already exists but creation was requested */
	else if ((flags & O_CREAT) && (flags & O_EXCL))
	{
		r->_errno = EEXIST;
		return -1;
	}

	/* the path is not a file */
	if (fobj->fsnode->type != NODE_FILE)
	{
		r->_errno = EINVAL;
		return -1;
	}

	/* initialize file position */
	fobj->pos = 0;

	return 0;
}


static int romfs_close(struct _reent *r, void *fd)
{
	/* nothing to do */
	return 0;
}

static ssize_t romfs_read(struct _reent *r, void *fd, char *ptr, size_t len)
{
	romfs_file_t *fobj = (romfs_file_t *)fd;	

	/* compute the position of read's end */
	size_t end_pos = fobj->pos + len;

	/* reached file's end */
	if(fobj->pos >= fobj->fsnode->size)
		return 0;

	/* attempting to read after file end, truncate len */
	if((fobj->pos + len) > fobj->fsnode->size)
		len = fobj->fsnode->size - fobj->pos;

	/* copy the requested content */
	OSBlockMove(ptr, fobj->fsnode->cont + fobj->pos, len, FALSE);

	/* advance position by bytes read */
	fobj->pos += len;

	/* Return read lenght */
	return len;
}

static off_t romfs_seek(struct _reent *r, void *fd, off_t pos, int dir)
{
	romfs_file_t *fobj = (romfs_file_t *)fd;
	off_t start;

	/* set relative seek start */
	switch (dir)
	{
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

	if(pos < 0)
	{
		/* attempting to seek before file start */
		if(start + pos < 0)
		{
			r->_errno = EINVAL;
			return -1;
		}
	}
	/* the position index overflows */
	else if(INT32_MAX - pos < start)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	/* set file position */
    fobj->pos = start + pos;

	/* return set position */
    return fobj->pos;
}

static int romfs_fstat(struct _reent *r, void *fd, struct stat *st)
{
	romfs_file_t *fobj = (romfs_file_t *)fd;

	/* clear stat structure */
	memset(st, 0, sizeof(struct stat));

	/* set known fields */
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
	/* attempt to find the correct node for the path */
	node_t *fsnode = node_get(ipath);

	/* the path doesn't exist */
	if (!fsnode)
	{
		r->_errno = ENOENT;
		return -1;
	}

	/* clear stat structure */
	memset(st, 0, sizeof(struct stat));

	/* set known fields */
	st->st_size  = fsnode->size;
	st->st_atime = st->st_mtime = st->st_ctime = fsnode->mtime;
	st->st_ino = fsnode->inode;
	st->st_mode  = (fsnode->type == NODE_FILE) ? ROMFS_FILE_MODE : ROMFS_DIR_MODE;
	st->st_nlink = 1;
	st->st_blksize = 512;
	st->st_blocks  = (st->st_blksize + 511) / 512;

	return 0;
}

static int romfs_chdir(struct _reent *r, const char *ipath)
{
	/* attempt to find the correct node for the path */
	node_t *fsdir = node_get(ipath);

	/* the path doesn't exist */
	if (!fsdir)
	{
		r->_errno = ENOENT;
		return -1;
	}

	/* the path is not a directory */
	if (fsdir->type != NODE_DIR)
	{
		r->_errno = EINVAL;
		return -1;
	}

	/* set current directory */
	fs_cwd = fsdir;

	return 0;
}

static DIR_ITER* romfs_diropen(struct _reent *r, DIR_ITER *dirState, const char *ipath)
{
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);

	/* attempt to find the correct node for the path */
	dirobj->fsdir = node_get(ipath);

	/* the path doesn't exist */
	if (!dirobj->fsdir)
	{
		r->_errno = ENOENT;
		return NULL;
	}

	/* the path is not a directory */
	if (dirobj->fsdir->type != NODE_DIR)
	{
		r->_errno = EINVAL;
		return NULL;
	}

	/* initialize directory iterator to the first entry */
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;

	return dirState;
}

static int romfs_dirreset(struct _reent *r, DIR_ITER *dirState)
{
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);

	/* reset directory iterator to the first entry */
	dirobj->ent = dirobj->fsdir->ent;
	dirobj->idx = 0;

	return 0;
}

static int romfs_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat)
{
	romfs_dir_t* dirobj = (romfs_dir_t*)(dirState->dirStruct);

	/* the first entry is current directory '.' */
	if (dirobj->idx == 0)
	{
		memset(filestat, 0, sizeof(*filestat));
		filestat->st_ino  = dirobj->fsdir->inode;
		filestat->st_mode = ROMFS_DIR_MODE;
		strcpy(filename, ".");
		dirobj->idx = 1;
		return 0;
	}

	/* the second entry is upper directory '..' */
	if (dirobj->idx == 1)
	{
		/* check for an upper directory */
		node_t* updir = dirobj->fsdir->up;

		/* we reached fs top, use fs_root */
		if (!updir)
			updir = &fs_root;

		memset(filestat, 0, sizeof(*filestat));
		filestat->st_ino = updir->inode;
		filestat->st_mode = ROMFS_DIR_MODE;
		strcpy(filename, "..");
		dirobj->idx = 2;
		return 0;
	}

	/* the directory is empty */	
	if (!dirobj->ent)
	{
		r->_errno = ENOENT;
		return -1;
	}

	/* write entry data */
	memset(filestat, 0, sizeof(*filestat));
	filestat->st_ino = dirobj->ent->inode;
	filestat->st_mode = (dirobj->ent->type == NODE_FILE) ? ROMFS_FILE_MODE : ROMFS_DIR_MODE;
	strcpy(filename, dirobj->ent->name);

	/* go to the next entry */
	dirobj->ent = dirobj->ent->next;

	return 0;
}

static int romfs_dirclose(struct _reent *r, DIR_ITER *dirState)
{
	/* nothing to do */
	return 0;
}

static devoptab_t romfs_devoptab =
{
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

#endif
