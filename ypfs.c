/**
CS 3210 Project 3 - YPFS
Author: Ho Pan Chan, Robert Harrison
**/


#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

static int ypfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path, ypfs_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(ypfs_str);
    }
    else
        res = -ENOENT;

    return res;
}

static int ypfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, ypfs_path + 1, NULL, 0);

    return 0;
}

static int ypfs_open(const char *path, struct fuse_file_info *fi)
{
    if(strcmp(path, ypfs_path) != 0)
        return -ENOENT;

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int ypfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    size_t len;
    (void) fi;
    if(strcmp(path, ypfs_path) != 0)
        return -ENOENT;

    len = strlen(ypfs_str);
    
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, ypfs_str + offset, size);
    } else
        size = 0;

    return size;
}

static int ypfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	//todo:implement
}

static struct fuse_operations ypfs_oper = {
    //.init        = ypfs_init,
    //.destroy     = ypfs_destroy,
    //.getattr     = ypfs_getattr,
    .fgetattr    = ypfs_fgetattr,
    //.access      = ypfs_access,
    //.readlink    = ypfs_readlink,
    .readdir     = ypfs_readdir,
    //.mknod       = ypfs_mknod,
    //.mkdir       = ypfs_mkdir,
    //.symlink     = ypfs_symlink,
    //.unlink      = ypfs_unlink,
    //.rmdir       = ypfs_rmdir,
    //.rename      = ypfs_rename,
    //.link        = ypfs_link,
    //.chmod       = ypfs_chmod,
    //.chown       = ypfs_chown,
    //.truncate    = ypfs_truncate,
    //.ftruncate   = ypfs_ftruncate,
    //.utimens     = ypfs_utimens,
    //.create      = ypfs_create,
    .open        = ypfs_open,
    .read        = ypfs_read,
    .write       = ypfs_write,
    //.statfs      = ypfs_statfs,
    //.release     = ypfs_release,
    //.opendir     = ypfs_opendir,
    //.releasedir  = ypfs_releasedir,
    //.fsync       = ypfs_fsync,
    //.flush       = ypfs_flush,
    //.fsyncdir    = ypfs_fsyncdir,
    //.lock        = ypfs_lock,
    //.bmap        = ypfs_bmap,
};

int main(int argc, char *argv[])
{
    umask(0);
    return fuse_main(argc, argv, &ypfs_oper, NULL);
}
