/**
CS 3210 Project 3 - YPFS
Author: Ho Pan Chan, Robert Harrison
**/

/*** uploading pics:
username
password
pic_name  (unique)
e.g. my_pic_2012_12_01.gif
in base64
****/

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
#include <sys/types.h>
#include <sys/stat.h>
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

typedef enum {YP_DIR, YP_PIC} YP_TYPE;

struct YP_NODE {
	char *name;
	char *hash; // unique name
	YP_TYPE type;
	struct YP_NODE ** children;
	struct YP_NODE* parent;
	int no_child;
	int open_count;
};

static struct YP_NODE *root_node;

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

struct YP_NODE* new_node(const char *path, YP_TYPE type, const char *hash) {
	struct YP_NODE *my_new_node = malloc(sizeof(struct YP_NODE));
	
	my_new_node->name = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(my_new_node->name, path);
	my_new_node->type = type;
	my_new_node->children = NULL;
	my_new_node->parent = NULL;
	my_new_node->no_child = 0;
	my_new_node->open_count = 0;
	my_new_node->hash = NULL;
	
	if (type == YP_PIC && hash == NULL) {
		my_new_node->hash = malloc(sizeof(char) * 50);
		sprintf(my_new_node->hash, "%ld", random() % 10000000000);
	} else if (type == YP_DIR && hash) {
		my_new_node->hash = malloc(sizeof(char) * 50);
		sprintf(my_new_node->hash, "%s", hash);
	}

	return my_new_node;
}

struct YP_NODE* add_child(struct YP_NODE *parent, struct YP_NODE *child) {
	struct YP_NODE** tmp_child_list;
	int tmp_no_child;
	int i;
	
	if (parent == NULL) {
		FSLog("Add child to a null parent");
		return NULL;
	}
	
	tmp_child_list = parent->children;
	tmp_no_child = parent->no_child;
	
	parent->children = malloc((tmp_no_child+1)*sizeof(struct YP_NODE*));
	
	for (i=0; i<tmp_no_child; i++) {
		(parent->children)[i] = tmp_child_list[i];
	}
	
	(parent->children)[tmp_no_child] = child;
	parent->no_child = tmp_no_child+1;
	
	child->parent = parent;
	
	free(tmp_child_list);
	
	return child;
}

void remove_child(struct YP_NODE* parent, struct YP_NODE* child) {
	struct YP_NODE** tmp_child_list;
	int tmp_no_child;
	int i, active;
	
	if (parent == NULL) {
		FSLog("Remove child to a null parent");
		return ;
	}
	
	tmp_child_list = parent->children;
	tmp_no_child = parent->no_child;
	
	if (tmp_no_child < 1) {
		FSLog("Parent has no child");
	} else {
		parent->children = malloc((tmp_no_child-1)*sizeof(struct YP_NODE*));

		for (i=0; i<tmp_no_child; i++) {
			if (tmp_child_list[i] != child) {
				(parent->children)[active] = tmp_child_list[i];
				active++;
			}
		}

		parent->no_child = tmp_no_child-1;

		free(tmp_child_list);

		if (child->name)
			free(child->name);

		if (child->hash)
			free(child->hash);

		if (child)
			free(child);
	}
		
}

void remove_node(struct YP_NODE *node) {
	remove_child(node->parent, node);
}

int path_depth(const char *path) {
	int i = 0;
	int j = 0;
	int k = strlen(path);
	
	for (j = 0; j<k-1; j++) {
		if (path[j] == '/')
			i++;
	}
	
	return i;
}


char* str_c(const char* path, char after)
{
	char *tmp = strchr(path, after);
	if (tmp != NULL)
		return tmp + 1;
	else
		return NULL;
}



struct YP_NODE* node_resolver(const char *path, struct YP_NODE *cur, int create, YP_TYPE type, char *hash, int skip_ext)
{
	char name[MAX_PATH_LENGTH];
	int i = 0;
	int last_node = 0;
	char *ext;
	char compare_name[MAX_PATH_LENGTH];
	char *curr_char;
	int n = 0;

	// FSLog("node_resolver");
	// FSLog(path);

	if (cur == NULL)
		FSLog("node for path: NULL cur");

	ext = str_c(path, '.');

	if (*path == '/')
		path++;

	// find the name
	while(*path && *path != '/' && ( ext == NULL || path < ext-1 || (skip_ext == 0))) {
		name[i] = *(path++);
		i++;
	}
	name[i] = '\0';

	if (*path == '\0') {
		last_node = 1;
		//FSLog("Last node");
	}
	if (i == 0) {
		//FSLog("return cur");
		return cur;
	}

	for (i = 0; i < cur->no_child; i++) {
		// compare the name
		
		ext = str_c((cur->children[i])->name, '.');
		curr_char = (cur->children[i])->name;
		n = 0;
		while(*curr_char != '\0' && (ext == NULL || (skip_ext == 0) || curr_char < ext-1)) {
			compare_name[n] = *(curr_char++);
			n++;
		}
		compare_name[n] = '\0';
		if (strcmp(name, compare_name) == 0)
			return node_resolver(path, cur->children[i], create, type, hash, skip_ext); // search it inside this child
		*compare_name = '\0';
	}


	if (create == 1) {
		// add a child to cur and continue the process
		return node_resolver(path, add_child(cur, new_node(name, last_node == 1 ? type : YP_DIR, hash)), create, type, hash, skip_ext);
	}

	return NULL;
}


struct YP_NODE* search_node(const char *path) {
	return node_resolver((char *)path, root_node, 0, 0, NULL, 0);
}

struct YP_NODE* search_node_no_extension(const char *path) {
	return node_resolver((char *)path, root_node, 0, 0, NULL, 1);
}

struct YP_NODE* create_node_from_path(const char *path, YP_TYPE type, char *hash) {
	return node_resolver((char *)path, root_node, 1, type, hash, 0);
}

void print_tree(struct YP_NODE* head, const char *pre) {
	char preN[20];
	char path_name[100];
	int i;
	sprintf(preN, "%s-", pre);
	sprintf(path_name, "%s%s", preN, head->name);
	FSLog(path_name);
	for (i=0; i<head->no_child; i++) {
		print_tree(head->children[i], preN);
	}
	
}

void print_full_tree() {
	int i;
	FSLog("Print tree");
	FSLog(root_node->name);
	for (i=0; i<root_node->no_child; i++) {
		print_tree(root_node->children[i], "-");
	}
	FSLog("End of Print");
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
	struct YP_NODE *my_node, *my_node_no_ext;

	FSLog("getattr");
	FSLog(path);
	
	my_node = search_node(path);
	my_node_no_ext = search_node_no_extension(path);
	
	if (my_node_no_ext == NULL) {
		return -ENOENT;
	}
	
	ypfs_fullpath(fpath, my_node_no_ext->name);
	
	if (my_node_no_ext && my_node_no_ext->type == YP_PIC && my_node_no_ext != my_node) {
		// convert here, so file 1324242 becomes 1324242.png
		// convert_img(my_node_no_ext, path);
		// for stat later in function
		strcat(fpath, strchr(path, '.'));
	}
	memset(stbuf, 0, sizeof(struct stat));
	
	if (my_node_no_ext->type == YP_DIR && strstr(path, "ypfs")) {
		// temp/ypfs/{some_dir}
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (my_node_no_ext->type == YP_DIR) {
		// temp/{some_dir} but not ypfs
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 2;
	} else if (my_node_no_ext != NULL && my_node != my_node_no_ext) {
		// get attr for non-original file ext
		ret = stat(fpath, stbuf);
		stbuf->st_mode = S_IFREG | 0444;
	} else if (my_node_no_ext != NULL) {
		ret = stat(fpath, stbuf);
		stbuf->st_mode = S_IFREG | 0444;
	} else {
		ret = -ENOENT;
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
/*
int ypfs_readlink(const char *path, char *link, size_t size)
{
    int ret = 0;

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

	FSLog("readlink");
    return ret;
}
*/

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int ypfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int ret = 0;
	FSLog("mknod");
    return ret;
}

/** Create a directory */
int ypfs_mkdir(const char* path, mode_t mode){
	int ret = 0;
	//char fpath[MAX_PATH_LENGTH];
	
	FSLog("mkdir");
	FSLog(path);
	//ypfs_fullpath(fpath, path);
	
	if (create_node_from_path(path, YP_DIR, NULL) == NULL)
		ret = -1;
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
	int i;
    (void) offset;
    (void) fi;
	struct YP_NODE *my_node;
	FSLog("readdir");
	FSLog(path);
	
	my_node = search_node(path);
	
	if (my_node == NULL)
		return -ENOENT;
	
	
	// put directories or files here mean display them

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
	for (i = 0; i< my_node->no_child; i++) {
		filler(buf, (my_node->children[i])->name, NULL, 0);
	}

    return 0;
}

/** Remove a file */
int ypfs_unlink(const char *path)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	struct YP_NODE *f_node = search_node(path);
	FSLog("unlink");
	FSLog(path);

	ypfs_fullpath(fpath, path);
	
	if (f_node != NULL)
		remove_node(f_node);
	
	ret = unlink(fpath);
	if (ret < 0)
		ret = -errno;
	
    return ret;
}


/** Remove a directory */
/*
int ypfs_rmdir(const char *path)
{
    int ret = 0;
 	char fpath[MAX_PATH_LENGTH];
	FSLog("rmdir");
	FSLog(path);
 	
 	ypfs_fullpath(fpath, path);
 	//     
 	ret = rmdir(fpath);
 	if (ret < 0)
		ret = -errno;
	
    return ret;
}
*/

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
/*
int ypfs_symlink(const char *path, const char *link)
{
    int ret = 0;
	char flink[MAX_PATH_LENGTH];
	FSLog("symlink");
	
	ypfs_fullpath(flink, link);
	 
	ret = symlink(path, flink);
	if (ret < 0)
		ret = -errno;
	
    return ret;
}
*/

/** Rename a file */
// both path and newpath are fs-relative
int ypfs_rename(const char *path, const char *newpath)
{
    int ret = 0;
	struct YP_NODE *old_n, *new_n;
	FSLog("rename");
	
	old_n = search_node(path);
	
	if (old_n == NULL)
		return -ENOENT;
		
	new_n = create_node_from_path(newpath, old_n->type, old_n->hash);
	
	if (new_n != old_n)
		remove_node(old_n);
	//ypfs_fullpath(fpath, path);
	//ypfs_fullpath(fnewpath, newpath);

    return 0;
}

/** Change the size of a file */
int ypfs_truncate(const char *path, off_t newsize)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	struct YP_NODE *f_node = search_node_no_extension(path), *r_node = search_node(path);
	FSLog("truncate");
	FSLog(path);
	
	ypfs_fullpath(fpath, path);
	
	if (f_node != r_node) {
		strcat(fpath, strchr(path, '.'));
	}
	
	/*
		char full_file_name[1000];
		NODE file_node = node_ignore_extension(path);
		NODE real_node = node_for_path(path);
		mylog("truncate");
		to_full_path(file_node->hash, full_file_name);
		if (file_node != real_node) {
		strcat(full_file_name, strchr(path, '.'));
		}
		return truncate(full_file_name, offset);
	
	*/
		
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
	FSLog("utimens");
	FSLog(path);
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
	int fd = -1, ret = 0;
	char fpath[MAX_PATH_LENGTH];
	struct YP_NODE *my_node;
	FSLog("open");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	FSLog(fpath);
	
	my_node = search_node_no_extension(path);
	
	if (my_node == NULL) {
		FSLog("Null my node");
		return -ENOENT;
	}
	
	/*
	if (strcmp( strstr(path, "."), strstr(my_node->name, ".") ) != 0) {
		strcat(fpath, strstr(path, "."));
	}
	*/
	
	FSLog(fpath);
	FSLog("Before real open");
	if (fi == NULL) {
		FSLog("FI is NULL");
	}
	//fd = open(fpath, fi->flags, 0666);
	fd = open("/tmp/ypfs/a.txt", fi->flags, 0666);
	
	if (fd < 0) {
		ret = -errno;
		FSLog("Fail in fd open");
		return ret;
	}
    fi->fh = fd;

	my_node->open_count++;
	FSLog("open done");
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
	struct YP_NODE *my_node;
	FSLog("read");
	
	my_node = search_node_no_extension(path);
	
	if (my_node == NULL)
		return -ENOENT;
	
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
	/*
	int ret = 0;
	
	FSLog("release");
	FSLog(path);
	
	ret = close(fi->fh);
	
	return ret;
	*/
	//ExifData *ed;
	//ExifEntry *entry;
	char full_file_name[1000];
	struct YP_NODE *f_node = search_node_no_extension(path);
	//char buf[1024];
	//struct tm file_time;
	char year[1024];
	char month[1024];
	char new_name[2048];

	FSLog("release");

	//to_full_path(f_node->hash, full_file_name);
	close(fi->fh);
	f_node->open_count--;

	// redetermine where the file goes
	if (f_node->open_count <= 0) {
		FSLog("file completely closed; checking if renaming necessary");
		// ed = exif_data_new_from_file(full_file_name);
		// if (ed) {
		// 	entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_DATE_TIME);
		// 	exif_entry_get_value(entry, buf, sizeof(buf));
		// 	mylog("Tag content:");
		// 	mylog(buf);
		// 	strptime(buf, "%Y:%m:%d %H:%M:%S", &file_time);
		// 	strftime(year, 1024, "%Y", &file_time);
		// 	strftime(month, 1024, "%B", &file_time);
		// 	sprintf(new_name, "/%s/%s/%s", year, month, file_node->name);
		// 	mylog(new_name);
		// 	ypfs_rename(path, new_name);
		// 	exif_data_unref(ed);
		// } else {
			int num_slashes = 0;
			int i;
			//time_t rawtime;

			for (i = 0; i < strlen(path); i++) {
				if (path[i] == '/')
				num_slashes++;
			}
			// in root
			if (num_slashes == 1) {
				FSLog("Inside num_slashes");
				struct stat sb;
				struct tm * pic_time;
				
				if (fstat(fi->fh, &sb) == -1) {
				        perror("stat");
				        exit(EXIT_FAILURE);
				}
				FSLog("Pass fstat");
				
				pic_time = localtime(&sb.st_ctime);
				strftime(year, 1024, "%Y", pic_time);
				strftime(month, 1024, "%B", pic_time);
				sprintf(new_name, "/%s/%s/%s", year, month, f_node->name);
				FSLog(new_name);
				ypfs_rename(path, new_name);

			}
		//}
	}

	return 0;
	
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
    
    int ret = -1;
	struct YP_NODE *my_node = search_node(path);
	//DIR *dp;
    char fpath[MAX_PATH_LENGTH];
	FSLog("opendir");
	FSLog(path);

    ypfs_fullpath(fpath, path);
   	
	if (my_node && my_node->type == YP_DIR)
		ret = 0;
	
    return ret;
}


/** Release directory
 *
 * Introduced in version 2.3
 */
/*
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
*/


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
    int ret = 0, i, num_slashes = 0;
	// struct YP_NODE *my_node;
 	char fpath[MAX_PATH_LENGTH], *filename = (char *)path;
 	int fd;

	FSLog("Create");
	FSLog(path);
	ypfs_fullpath(fpath, path);
	FSLog(fpath);
	
	while(*filename != '\0') filename++;
	while(*filename != '/' && filename >= path) filename--;
	if (*filename == '/') filename++;
	
	for (i = 0; i< strlen(path); i++) {
		if (path[i] == '/')
			num_slashes++;
	}
	
	if (strcmp(filename, "debugtree") == 0) {
		print_full_tree();
		return -1;
	}
	
	if (num_slashes > 1 || strstr(path, "/ypfs")) {
		return -1;
	}

	FSLog("Create test pass");
	
	// my_node = new_node(filename, YP_PIC, NULL);
	// 	if (my_node != NULL)
	// 		add_child(root_node, my_node);
	create_node_from_path(path, YP_PIC, NULL);
	FSLog("creat");
	// fd = creat("/tmp/ypfs/a.txt", mode);
	// 	FSLog("creat pass");
	// 	if (fd < 0)
	// 		return -errno;
	// 		
	// 	fi->fh = fd;
	// 	
	// 	return fd;
	
	return ypfs_open(path, fi);
/*



	if (0 == strcmp(end, "debugtwitter")) {
	num_urls = twitter_get_img_urls("team_initrd", urls, 128);
	mylog("TWITTER URLS");
	for (i = 0; i < num_urls; i++) {
	mylog(urls[i]);
	free(urls[i]);
	}
	return -1;
	}

	if (0 == strcmp(end, "debugserialize")) {
	serialize();
	return -1;
	}

	if (0 == strcmp(end, "debugdeserialize")) {
	deserialize();
	return -1;
	}

*/
	
	
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




struct fuse_operations ypfs_oper = {
    .getattr	= ypfs_getattr,
	.readdir	= ypfs_readdir,
	.open		= ypfs_open,
	.read		= ypfs_read,
	.create		= ypfs_create,
	.write		= ypfs_write,
	.utimens	= ypfs_utimens,
	.mknod		= ypfs_mknod,
	.release	= ypfs_release,
	.truncate	= ypfs_truncate,
	.unlink		= ypfs_unlink,
	.rename		= ypfs_rename,
	.mkdir		= ypfs_mkdir,
	.opendir	= ypfs_opendir,
	.init		= ypfs_init,
	.destroy	= ypfs_destroy
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
	
	root_node = new_node("/", YP_DIR, NULL);
	create_node_from_path("/ypfs", YP_DIR, NULL);
	
	FSLog("about to call fuse_main");
    fuse_ret = fuse_main(argc, argv, &ypfs_oper, ypfs_data);
	//FSLog("fuse_main:");
	//FSLog(fuse_ret);
	
	return fuse_ret;
}
