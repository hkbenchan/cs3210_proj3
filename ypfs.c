/**
CS 3210 Project 3 - YPFS
Author: Ho Pan Chan, Robert Harrison
**/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


static const char *ypfs_str = "Welecome to your pic filesystem!\n";
static const char *ypfs_path = "/ypfs";
static char username[30];

static void FSLog(const char *message)
{
	FILE *fh;
	struct timeval t;
	gettimeofday(&t, NULL);
	
	fh = fopen("/tmp/ypfs/log","a");
	fprintf(fh, "%ld.%ld : %s\n", t.tv_sec, t.tv_usec, message);
	fclose(fh);
	
}


static int ypfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path, ypfs_path) == 0) {
        stbuf->st_mode = S_IFDIR | 0644;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
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

	FSLog("readdir");
	FSLog(path);
	
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
	//char tmp[100];
    //size_t len;
    //(void) fi;
	/*
    if(strcmp(path, ypfs_path) != 0)
        return -ENOENT;

	strcpy(tmp, ypfs_str);
	strcat(tmp, username);
	strcat(tmp, "\n");
    len = strlen(tmp);
    
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, tmp + offset, size);
    } else
        size = 0;

    return size;
	*/
	return -ENOENT; // directory cannot be read
}

static int ypfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	//todo:implement
	FSLog("write trigger");
	return 0;
}

static int ypfs_mkdir(const char* path, mode_t mode){
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int ypfs_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int ypfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static void ypfs_destroy() {
	
	//printf("Bye bye %s\n", username);
	FSLog("---End---");
	return ;
}

static struct fuse_operations ypfs_oper = {
    //.init        = ypfs_init,
    .destroy     = ypfs_destroy,
    .getattr     = ypfs_getattr,
    //.fgetattr    = ypfs_fgetattr,
    //.access      = ypfs_access,
    //.readlink    = ypfs_readlink,
    .readdir     = ypfs_readdir,
    //.mknod       = ypfs_mknod,
    .mkdir       = ypfs_mkdir,
    //.symlink     = ypfs_symlink,
    //.unlink      = ypfs_unlink,
    //.rmdir       = ypfs_rmdir,
    //.rename      = ypfs_rename,
    //.link        = ypfs_link,
    .chmod       = ypfs_chmod,
    .chown       = ypfs_chown,
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
	FSLog("---Start---");
    umask(0);
	printf("Username: ");
	scanf("%s", username);
    return fuse_main(argc, argv, &ypfs_oper, NULL);
}
