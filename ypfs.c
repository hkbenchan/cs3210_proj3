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



static struct fuse_operations ypfs_oper = {
    .init        = ypfs_init,
    .destroy     = ypfs_destroy,
    .getattr     = ypfs_getattr,
    .fgetattr    = ypfs_fgetattr,
    .access      = ypfs_access,
    .readlink    = ypfs_readlink,
    .readdir     = ypfs_readdir,
    .mknod       = ypfs_mknod,
    .mkdir       = ypfs_mkdir,
    .symlink     = ypfs_symlink,
    .unlink      = ypfs_unlink,
    .rmdir       = ypfs_rmdir,
    .rename      = ypfs_rename,
    .link        = ypfs_link,
    .chmod       = ypfs_chmod,
    .chown       = ypfs_chown,
    .truncate    = ypfs_truncate,
    .ftruncate   = ypfs_ftruncate,
    .utimens     = ypfs_utimens,
    .create      = ypfs_create,
    .open        = ypfs_open,
    .read        = ypfs_read,
    .write       = ypfs_write,
    .statfs      = ypfs_statfs,
    .release     = ypfs_release,
    .opendir     = ypfs_opendir,
    .releasedir  = ypfs_releasedir,
    .fsync       = ypfs_fsync,
    .flush       = ypfs_flush,
    .fsyncdir    = ypfs_fsyncdir,
    .lock        = ypfs_lock,
    .bmap        = ypfs_bmap,
    .ioctl       = ypfs_ioctl,
    .poll        = ypfs_poll,
#ifdef HAVE_SETXATTR
    .setxattr    = ypfs_setxattr,
    .getxattr    = ypfs_getxattr,
    .listxattr   = ypfs_listxattr,
    .removexattr = ypfs_removexattr,
#endif
    .flag_nullpath_ok = 0,                /* See below */
};

int main(int argc, char *argv[])
{
    umask(0);
    return fuse_main(argc, argv, &ypfs_oper, NULL);
}
