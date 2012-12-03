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
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#define DEBUG 1
#define SERCET_LOCATION "/tmp/ypfs/.config"
#define MAX_PATH_LENGTH 500

#define CURRENT_SESSION ((struct ypfs_session *) fuse_get_context()->private_data)

struct ypfs_session {
	char username[30];
    char *private_key_location;
	char *public_key_location;
	char *mount_point;
};

const char *ypfs_str = "Welecome to your pic filesystem!\n";
const char *ypfs_path = "/ypfs";
char username[30];

static void ypfs_fullpath(char fpath[MAX_PATH_LENGTH], const char *path)
{

	strcpy(fpath, CURRENT_SESSION->mount_point);
    strncat(fpath, path, MAX_PATH_LENGTH);

}


void FSLogFlush()
{
	FILE *fh;
	fh = fopen("/tmp/ypfs/log","w");
	fclose(fh);
}

void FSLog(const char *message)
{
	FILE *fh;
	struct timeval t;
	gettimeofday(&t, NULL);
	
	fh = fopen("/tmp/ypfs/log","a");
	if (fh != NULL) {
		fprintf(fh, "%ld.%ld : %s\n", t.tv_sec, t.tv_usec, message);
		fclose(fh);
	}
	
}

int find_my_config()
{
	int ret = 0;
	FILE *fh;
	fh = fopen(SERCET_LOCATION,"r");
	
	if (fh != NULL) {
		
		ret = fscanf(fh,"%s", username);
		fclose(fh);
		
		if ( strcmp(username, "") != 0) {
			// catch the username, it is good
			ret = 1;
		}
	}
	
	return ret;
}

int make_my_config()
{
	int ret = 0;
	FILE *fh;
	fh = fopen(SERCET_LOCATION, "w");
	
	if (fh != NULL) {
		fprintf(fh , "%s", username);
		fclose(fh);
		
		/****** Send key request to server and store **********/
		
		ret = 1;
		
		
		
	} else {
		printf("Fail to create config...\nPlease ensure you are admin.\nExit the system!\n");
	}

	return ret;
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */

int ypfs_getattr(const char *path, struct stat *stbuf)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];

	FSLog("getattr");
	FSLog(path);
	
    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
		stbuf->st_size = 4096; // I'm directory
    }
    else if(strcmp(path, ypfs_path) == 0) {
        stbuf->st_mode = S_IFDIR | 0644;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
    }
    else {
		ypfs_fullpath(fpath, path);
		ret = lstat(fpath, stbuf);
	}
    return ret;
}


/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int ypfs_readlink(const char *path, char *link, size_t size)
{
    int ret = 0;
/*
    char fpath[MAX_PATH_LENGTH];
    
    log_msg("bb_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    bb_fullpath(fpath, path);
    
    ret = readlink(fpath, link, size - 1);
    if (ret < 0)
	ret = bb_error("bb_readlink readlink");
    else  {
	link[ret] = '\0';
	ret = 0;
    }
*/
	FSLog("readlink");
    return ret;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int ypfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int ret = 0;
    /*char fpath[MAX_PATH_LENGTH];
    
    log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    bb_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, rdev)' but this
    //  is more portable
    if (S_ISREG(mode)) {
        ret = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
	if (ret < 0)
	    ret = bb_error("bb_mknod open");
        else {
            ret = close(ret);
	    if (ret < 0)
		ret = bb_error("bb_mknod close");
	}
    } else
	if (S_ISFIFO(mode)) {
	    ret = mkfifo(fpath, mode);
	    if (ret < 0)
		ret = bb_error("bb_mknod mkfifo");
	} else {
	    ret = mknod(fpath, mode, dev);
	    if (ret < 0)
		ret = bb_error("bb_mknod mknod");
	}
    */
	FSLog("mknod");
    return ret;
}

/** Create a directory */
int ypfs_mkdir(const char* path, mode_t mode){
	int ret;
	char fpath[MAX_PATH_LENGTH];
	
	FSLog("mkdir");
	FSLog(path);
	ypfs_fullpath(fpath, path);

	ret = mkdir(fpath, mode);
	if (ret < 0)
		return -errno;

	return ret;
}



/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int ypfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

	FSLog("readdir");
	FSLog(path);
	
	int match = 0;
	
    if(strcmp(path, "/") == 0) {
		match++;
		filler(buf, ypfs_path + 1, NULL, 0);
	} else if (strcmp(path, ypfs_path) == 0) {
		match++;
	}
	
	if (match == 0) {
		return -ENOENT;
	}

	// put directories or files here mean display them

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    

    return 0;
}

/** Remove a file */
int ypfs_unlink(const char *path)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	FSLog("unlink");
	FSLog(path);
	//     
	//     log_msg("bb_unlink(path=\"%s\")\n",
	//     path);
	ypfs_fullpath(fpath, path);
	//     
	ret = unlink(fpath);
	if (ret < 0)
		ret = -errno;
	
    return ret;
}

/** Remove a directory */
int ypfs_rmdir(const char *path)
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH];
	FSLog("rmdir");
	FSLog(path);
 	//     
 	//     log_msg("bb_rmdir(path=\"%s\")\n",
 	//     path);
 	ypfs_fullpath(fpath, path);
 	//     
 	ret = rmdir(fpath);
 	if (ret < 0)
		ret = -errno;
	
    return ret;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int ypfs_symlink(const char *path, const char *link)
{
    int ret = 0;
	char flink[MAX_PATH_LENGTH];
	FSLog("symlink");
	//     
	//     log_msg("\nbb_symlink(path=\"%s\", link=\"%s\")\n",
	//     path, link);
	ypfs_fullpath(flink, link);
	//     
	ret = symlink(path, flink);
	if (ret < 0)
		ret = -errno;
	
    return ret;
}

/** Rename a file */
// both path and newpath are fs-relative
int ypfs_rename(const char *path, const char *newpath)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH], fnewpath[MAX_PATH_LENGTH];
	FSLog("rename");
	//     
	//     log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n",
	//     path, newpath);
	ypfs_fullpath(fpath, path);
	ypfs_fullpath(fnewpath, newpath);
	//     
	ret = rename(fpath, fnewpath);
	if (ret < 0)
		ret = -errno;
		
	
    return ret;
}

/** Create a hard link to a file */
int ypfs_link(const char *path, const char *newpath)
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH], fnewpath[MAX_PATH_LENGTH];
	FSLog("link");

 	//     
 	//     log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n",
 	//     path, newpath);
    ypfs_fullpath(fpath, path);
    ypfs_fullpath(fnewpath, newpath);
 	//     
 	ret = link(fpath, fnewpath);
 	if (ret < 0)
 		ret = -errno;
	
    return ret;
}

/** Change the permission bits of a file */
int ypfs_chmod(const char *path, mode_t mode)
{
	int res;
	char fpath[MAX_PATH_LENGTH];
	FSLog("chmod");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	
	res = chmod(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

/** Change the owner and group of a file */
int ypfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char fpath[MAX_PATH_LENGTH];
	FSLog("chown");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	
	res = lchown(fpath, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

/** Change the size of a file */
int ypfs_truncate(const char *path, off_t newsize)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	
	FSLog("truncate");
	FSLog(path);
	//     
	//     log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n",
	//     path, newsize);
	ypfs_fullpath(fpath, path);
	//     
	ret = truncate(fpath, newsize);
	if (ret < 0)
		ret = -errno;
		
    return ret;
}

/** Change the access and/or modification times of a file */
/** Change the access and modification times of a file with nanosecond resolution */
int ypfs_utimens(const char *path, const struct timespec tv[2])
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH];
	FSLog("utimens");
	FSLog(path);
 	//     
 	//     log_msg("\nbb_utime(path=\"%s\", ubuf=0x%08x)\n",
 	//     path, ubuf);
 	ypfs_fullpath(fpath, path);
 	    
 	ret = utime(fpath, ubuf);
 	if (ret < 0)
 		ret = -errno
	
    return ret;
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int ypfs_open(const char *path, struct fuse_file_info *fi)
{
	int fd, ret = 0;
	char fpath[MAX_PATH_LENGTH];
	FSLog("open");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	
	// if((fi->flags & 3) != O_RDONLY)
	//         return -EACCES;
	// 
	// 	if (strcmp(path, "/") == 0) {
	// 		fd = open(path, fi->flags);
	// 	}
	// 
	// 	else if(strcmp(path, ypfs_path) == 0) {
	// 		fd = open(path, fi->flags);
	// 	} else
	//         return -ENOENT;
	// 
	// 	if ( fd == -1 )
	// 		return -errno;
	
	fd = open(fpath, fi->flags);
    if (fd < 0)
		ret = -errno;

    fi->fh = fd;

    return ret;
}


/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int ypfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
	int ret = 0;
	FSLog("read");
	ret = pread(fi->fh, buf, size, offset);
	
	if (ret < 0)
		ret = -errno;
	
	return ret;
}


/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int ypfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	
	int ret = 0;
	FSLog("write");
	FSLog(path);
	
	ret = pwrite(fi->fh, buf, size, offset);
	
	if (ret < 0)
		ret = -errno;
		
	return ret;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int ypfs_statfs(const char *path, struct statvfs *statv)
{
    int ret = 0;
    char fpath[MAX_PATH_LENGTH];
    //     
    //     log_msg("\nbb_statfs(path=\"%s\", statv=0x%08x)\n",
    // 	    path, statv);
	FSLog("statfs");
	FSLog(path);
    ypfs_fullpath(fpath, path);
    //     
    // get stats for underlying filesystem
    ret = statvfs(fpath, statv);
    if (ret < 0)
     	ret = -errno;
    //     
    //     log_statvfs(statv);
	
    return ret;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int ypfs_flush(const char *path, struct fuse_file_info *fi)
{
    int ret = 0;
    FSLog("flush");
    // log_msg("\nbb_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    //     // no need to get fpath on this one, since I work from fi->fh not the path
    //     log_fi(fi);
	
    return ret;
}


/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int ypfs_release(const char *path, struct fuse_file_info *fi){
	
	// this one is called when a file is closed
	// handle the file here
	
	// TO DO:
	int ret = 0;
	
	FSLog("release");
	FSLog(path);
	
	ret = close(fi->fh);
	
	return ret;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int ypfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int ret = 0;
    
    // log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
    // 	    path, datasync, fi);
    //     log_fi(fi);
    //     
    //     if (datasync)
    // 	ret = fdatasync(fi->fh);
    //     else
    // 	ret = fsync(fi->fh);
    //     
    //     if (ret < 0)
    // 	bb_error("bb_fsync fsync");
	FSLog("fsync");
    return ret;
}

/** Set extended attributes */
int ypfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	FSLog("setxattr");
	FSLog(path);
	//     
	//     log_msg("\nbb_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	//     path, name, value, size, flags);
	ypfs_fullpath(fpath, path);
	//     
	ret = lsetxattr(fpath, name, value, size, flags);
	if (ret < 0)
		ret = -errno;
	
    return ret;
}

/** Get extended attributes */
int ypfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	FSLog("getxattr");
	FSLog(path);
	//     
	//     log_msg("\nbb_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	//     path, name, value, size);
	ypfs_fullpath(fpath, path);
	//     
	ret = lgetxattr(fpath, name, value, size);
	if (ret < 0)
		ret = -errno;
	//     else
	// log_msg("    value = \"%s\"\n", value);
	
    return ret;
}


/** List extended attributes */
int ypfs_listxattr(const char *path, char *list, size_t size)
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH];
 	char *ptr;
	FSLog("listxattr");
	FSLog(path);
 	//     
 	//     log_msg("bb_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
 	//     path, list, size
 	//     );
 	ypfs_fullpath(fpath, path);
 	//     
 	ret = llistxattr(fpath, list, size);
 	if (ret < 0)
 		ret = -errno;
 	//     
 	//     log_msg("    returned attributes (length %d):\n", ret);
 	//     for (ptr = list; ptr < list + ret; ptr += strlen(ptr)+1)
 	// log_msg("    \"%s\"\n", ptr);

    return ret;
}

/** Remove extended attributes */
int ypfs_removexattr(const char *path, const char *name)
{
    int ret = 0;
    char fpath[MAX_PATH_LENGTH];
	FSLog("removexattr");
    //     
    //     log_msg("\nbb_removexattr(path=\"%s\", name=\"%s\")\n",
    // 	    path, name);
    ypfs_fullpath(fpath, path);
    //     
    ret = lremovexattr(fpath, name);
    if (ret < 0)
    	ret = -errno;
	
    return ret;
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int ypfs_opendir(const char *path, struct fuse_file_info *fi)
{
    
    int ret = 0;
	DIR *dp;
    char fpath[MAX_PATH_LENGTH];
	FSLog("opendir");
	FSLog(path);
    //     
    //     log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n",
    // 	  path, fi);
    ypfs_fullpath(fpath, path);
    //     
    dp = opendir(fpath);
    if (dp == NULL)
     	ret = -errmp;
    
    fi->fh = (intptr_t) dp;
        
    //     log_fi(fi);
	
    return ret;
}


/** Release directory
 *
 * Introduced in version 2.3
 */
int ypfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int ret = 0;
    FSLog("releasedir");
    // log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n",
    // 	    path, fi);
    //    log_fi(fi);
    //    
    closedir((DIR *) (uintptr_t) fi->fh);
	
    return ret;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int ypfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int ret = 0;
    FSLog("fsyncdir");
    // log_msg("\nbb_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
    // 	    path, datasync, fi);
    //     log_fi(fi);
	
    return ret;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *ypfs_init(struct fuse_conn_info *conn)
{
	FSLog("init");
    //log_msg("\nbb_init()\n");
    
	return (struct ypfs_session *)fuse_get_context()->private_data;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void ypfs_destroy(void *userdata) {
	
	//printf("Bye bye %s\n", username);
	
	// deallocate the object and free
	free(CURRENT_SESSION->mount_point);
	// free(CURRENT_SESSION->public_key_location);
	// free(CURRENT_SESSION->private_key_location);
	free(CURRENT_SESSION);
	
	FSLog("---End---");
	return ;
}


/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int ypfs_access(const char *path, int mask)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	FSLog("access");
	FSLog(path);
	//    
	//     log_msg("\nbb_access(path=\"%s\", mask=0%o)\n",
	//     path, mask);
	ypfs_fullpath(fpath, path);
	//     
	ret = access(fpath, mask);
	//     
	if (ret < 0)
		ret = -errno
    return ret;
}


/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int ypfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH];
 	int fd;

	FSLog("Create");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	FSLog(fpath);
	
	fd = creat(fpath, mode);
	FSLog("Create end");
	
	if (fd < 0)
		ret = -errno;

	fi->fh = fd;
 	//     
 	//     log_msg("\nbb_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
 	//     path, mode, fi);
 	//     bb_fullpath(fpath, path);
 	//     
 	//     fd = creat(fpath, mode);
 	//     if (fd < 0)
 	// ret = bb_error("bb_create creat");
 	//     
 	//     fi->fh = fd;
 	//     
 	//     log_fi(fi);

	
    return ret;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int ypfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
	FSLog("ftruncate");
    // log_msg("\nbb_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
    // 	    path, offset, fi);
    //     log_fi(fi);
    //     
    ret = ftruncate(fi->fh, offset);
    if (ret < 0)
		ret = -errno;
    return ret;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
// Since it's currently only called after bb_create(), and bb_create()
// opens the file, I ought to be able to just use the fd and ignore
// the path...
int ypfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int ret = 0;
    FSLog("fgetattr");
	FSLog(path);
	ret = fstat(fi->fh, statbuf);
	
	if (ret < 0)
		ret = -errno;
 	//    log_msg("\nbb_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
 	//     path, statbuf, fi);
 	//     log_fi(fi);
 	//     
 	//     ret = fstat(fi->fh, statbuf);
 	//     if (ret < 0)
 	// ret = bb_error("bb_fgetattr fstat");
 	//     
 	//     log_stat(statbuf);
    	
    return ret;
}



/**
* Perform POSIX file locking operation
* 
* The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
* For the meaning of fields in 'struct flock' see the man page for fcntl(2). 
* The l_whence field will always be set to SEEK_SET.
* 
* For checking lock ownership, the 'fuse_file_info->owner' argument must be used.
* For F_GETLK operation, the library will first check currently held locks, and 
* if a conflicting lock is found it will return information without calling 
* this method. This ensures, that for local locks the l_pid field 
* is correctly filled in. The results may not be accurate in case 
* of race conditions and in the presence of hard links, but 
* it's unlikly that an application would rely on accurate 
* GETLK results in these cases. If a conflicting lock is not 
* found, this method will be called, and the filesystem may 
* fill out l_pid by a meaningful value, or it may leave this field zero.
*
* For F_SETLK and F_SETLKW the l_pid field will be set to the pid 
* of the process performing the locking operation.
*
* Note: if this method is not implemented, the kernel will still 
* allow file locking to work locally. Hence it is only interesting 
* for network filesystems and similar.
*
* Introduced in version 2.6
*/
/*
int ypfs_lock(const char *, struct fuse_file_info *fi, int cmd, struct flock *)
{
	int ret = 0;
	
	
	return ret;
}
*/

struct fuse_operations ypfs_oper = {
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
	.setxattr 	 = ypfs_setxattr,
  	.getxattr 	 = ypfs_getxattr,
	.listxattr	 = ypfs_listxattr,
	.removexattr = ypfs_removexattr,
    // .lock        = ypfs_lock, // no idea on how it works
    // .bmap        = ypfs_bmap, // we don't need this
};

int main(int argc, char *argv[])
{
	int private_file_exists = 0;
	struct ypfs_session *ypfs_data;
	int fuse_ret = 0;
	int i;
	if (DEBUG == 0)
		FSLogFlush();
	
	if (argc<2) {
		printf("./ypfs MOUNT_POINT");
		return -1;
	}
	
	FSLog("---Start---");
	// check if private file exists
	private_file_exists = find_my_config();
	
	ypfs_data = malloc(sizeof(struct ypfs_session));
	
	if (ypfs_data == NULL) {
		FSLog("malloc error");
		return -1;
	}
	
	umask(0);
	
	if (!private_file_exists) {
		printf("This is your first time to use this system, please register...\nUsername: ");
		scanf("%s", username);
		if (make_my_config() == 0) {
			return -1;
		}
		
		//ypfs_data->mount_point = realpath(argv[], NULL);
	}
	
	ypfs_data->mount_point = realpath(argv[1], NULL);
	//printf("Your mount point: %s\n", ypfs_data->mount_point);
	printf("Welcome %s!\n", username);
	
	strcpy(ypfs_data->username ,username);
	
	FSLog("about to call fuse_main");
    fuse_ret = fuse_main(argc, argv, &ypfs_oper, ypfs_data);
	//FSLog("fuse_main:");
	//FSLog(fuse_ret);
	
	return fuse_ret;
}
