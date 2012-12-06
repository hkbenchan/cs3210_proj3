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

// externel lib
#include <libexif/exif-data.h>
#include <curl/curl.h>

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#define DEBUG 0
#define SERCET_LOCATION "/tmp/.config_ypfs"
#define LOGFILE_LOCATION "/tmp/ypfs.log"
#define TREE_LOCATION "/tmp/ypfs.tree"

#define MAX_PATH_LENGTH 500

#define CURRENT_SESSION ((struct ypfs_session *) fuse_get_context()->private_data)
// allow extension: .gif, .jpg, .png
typedef unsigned char uchar;

struct ypfs_session {
	char username[30];
	char password[25];
	uchar cipertext[100];
	char *mount_point;
};

typedef enum {YP_DIR, YP_PIC} YP_TYPE;

struct YP_NODE {
	char *name;
	//char *hash; // unique name
	YP_TYPE type;
	struct YP_NODE ** children;
	struct YP_NODE* parent;
	int no_child;
	int open_count;
	int private;
	int year;
	int month;
};

static struct YP_NODE *root_node;

const char *ypfs_str = "Welecome to your pic filesystem!\n";
const char *ypfs_path = "/ypfs";
char username[30], password[25];
uchar cipertext[100];

void remove_self_and_children_file(struct YP_NODE *);
int ypfs_release(const char *, struct fuse_file_info *);
void my_curl_photo_upload(char *, struct YP_NODE*);
void my_curl_photo_download(char *, struct YP_NODE*);

static void ypfs_fullpath(char fpath[MAX_PATH_LENGTH], const char *path)
{

	strcpy(fpath, CURRENT_SESSION->mount_point);
    strncat(fpath, path, MAX_PATH_LENGTH);

}

static void ypfs_switchpath(char fpath[MAX_PATH_LENGTH], const char *path)
{
	strcpy(fpath, "/tmp/ypfs/");
	strncat(fpath, path, MAX_PATH_LENGTH);
}

void FSLogFlush()
{
	FILE *fh;
	fh = fopen(LOGFILE_LOCATION,"w");
	fclose(fh);
}

void FSLog(const char *message)
{
	FILE *fh;
	struct timeval t;
	gettimeofday(&t, NULL);
	
	fh = fopen(LOGFILE_LOCATION,"a");
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
		
		ret = fscanf(fh,"%s\n%s\n%s", username, password, (char *) cipertext);
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
		fprintf(fh , "%s\n%s\n%s", username, password, (char *) cipertext);
		fclose(fh);
		
		/****** Send key request to server and store **********/
		
		ret = 1;		
		
	} else {
		printf("Fail to create config...\nPlease ensure you are admin.\nExit the system!\n");
	}

	return ret;
}

void Encrypt(uchar *in, uchar *out)
{
    AES_KEY encryptKey;

    AES_set_encrypt_key(cipertext, 256, &encryptKey);

    AES_ecb_encrypt(in, out, &encryptKey, AES_ENCRYPT);

}

void Decrypt(uchar *in, uchar *out)
{
    AES_KEY decryptKey;
    
	AES_set_decrypt_key(cipertext, 256, &decryptKey);

    AES_ecb_encrypt(in, out, &decryptKey, AES_DECRYPT);

}

struct YP_NODE* new_node(const char *path, YP_TYPE type) {
	struct YP_NODE *my_new_node = malloc(sizeof(struct YP_NODE));
	
	my_new_node->name = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(my_new_node->name, path);
	my_new_node->name[strlen(path)] = '\0';
	my_new_node->type = type;
	my_new_node->children = NULL;
	my_new_node->parent = NULL;
	my_new_node->no_child = 0;
	my_new_node->open_count = 0;
	my_new_node->private = 0;
	my_new_node->year = 0;
	my_new_node->month = 0;
	
	fprintf(stderr, "***********New node created with name: %s\n", my_new_node->name);
	
	/*
	my_new_node->hash = NULL;
	
	if (type == YP_PIC && hash == NULL) {
		my_new_node->hash = malloc(sizeof(char) * 50);
		sprintf(my_new_node->hash, "%ld", random() % 10000000000);
	} else if (type == YP_DIR && hash) {
		my_new_node->hash = malloc(sizeof(char) * 50);
		sprintf(my_new_node->hash, "%s", hash);
	}
	*/
	return my_new_node;
}

struct YP_NODE* add_child(struct YP_NODE *parent, struct YP_NODE *child) {
	struct YP_NODE** tmp_child_list;
	int tmp_no_child;
	int i;
	
	if (parent == NULL) {
		fprintf(stderr, "***********Add child to a null parent\n");
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
	int i, active = 0;
	
	if (parent == NULL) {
		fprintf(stderr, "***********Remove child to a null parent\n");
		return ;
	}
	
	if (parent->no_child < 1) {
		fprintf(stderr, "***********Parent has no child\n");
	} else {
		tmp_child_list = parent->children;
		tmp_no_child = parent->no_child;
		
		if (tmp_no_child == 1) {
			parent->children = NULL;
			fprintf(stderr, "***********Remove child: parent %s now have 0 child\n", parent->name);
		} else {
			parent->children = malloc((tmp_no_child-1)*sizeof(struct YP_NODE*));

			for (i=0; i<tmp_no_child; i++) {
				if (tmp_child_list[i] != child) {
					parent->children[active] = tmp_child_list[i];
					active++;
				}
			}
		}

		parent->no_child = tmp_no_child-1;

		free(tmp_child_list);

		if (child->name)
			free(child->name);		
		
		if (child)
			free(child);
	}
		
}

void remove_node(struct YP_NODE *node) {
	fprintf(stderr, "***********Remove node %s from parent %s\n", node->name, node->parent->name);
	remove_child(node->parent, node);
}

void remove_self_and_children_file(struct YP_NODE *parent) {
	int i;
	char absolute_path[MAX_PATH_LENGTH];
	
	for (i=0; i<parent->no_child; i++) {
		remove_self_and_children_file(parent->children[i]);
	}
	
	// remove self file
	ypfs_switchpath(absolute_path, parent->name);
	fprintf(stderr, "***********remove: %s\n", absolute_path);
	
	if (parent->type == YP_DIR)
		rmdir(absolute_path);
	else
		unlink(absolute_path);
}

void remove_copied_files() {
	fprintf(stderr, "***********Start removing...\n");
	remove_self_and_children_file(root_node);
	fprintf(stderr, "***********Finish removing...\n");
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

char* cut_extension(const char* filename)
{
	if (filename != NULL) {
		int i, cnt = 0;
		for (i=0; i<strlen(filename); i++){
			if (filename[i] == '.') {
				break;
			}
			cnt++;
		}
		if (cnt == 0) {
			return '\0';
		} else {
			int j = 0;
			char *no_ext = malloc((cnt+1) * sizeof(char));
			for (j = 0; j<cnt; j++) {
				no_ext[j] = filename[j];
			}
			no_ext[cnt] = '\0';
			return no_ext;
		}
		
	} else {
		return NULL;
	}
}


struct YP_NODE* node_resolver(const char *path, struct YP_NODE *cur, int create, YP_TYPE type, int skip_ext)
{
	char name[MAX_PATH_LENGTH];
	int i = 0;
	int last_node = 0;
	char *ext;
	char compare_name[MAX_PATH_LENGTH];
	char *curr_char;
	int n = 0;

	fprintf(stderr, "***********node_resolver: %s\n", path);

	if (cur == NULL) {
		fprintf(stderr, "***********node for path: NULL cur\n");
	}
		

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
		fprintf(stderr, "***********Last node\n");
	}
	if (i == 0) {
		fprintf(stderr, "***********return cur %s\n", cur->name);
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
		if (strcmp(name, compare_name) == 0) {
			fprintf(stderr, "***********In %s, node %s found, go inside found child %s\n", cur->name, (cur->children[i])->name, name);
			return node_resolver(path, cur->children[i], create, type, skip_ext); // search it inside this child
		}
			
		*compare_name = '\0';
	}


	if (create == 1) {
		// add a child to cur and continue the process
		fprintf(stderr, "***********add child %s\n", name);
		return node_resolver(path, add_child(cur, new_node(name, last_node == 1 ? type : YP_DIR)), create, type, skip_ext);
	}

	return NULL;
}


struct YP_NODE* search_node(const char *path) {
	return node_resolver(path, root_node, 0, 0, 0);
}

struct YP_NODE* search_node_no_extension(const char *path) {
	return node_resolver(path, root_node, 0, 0, 1);
}

struct YP_NODE* create_node_from_path(const char *path, YP_TYPE type) {
	return node_resolver(path, root_node, 1, type, 0);
}

void print_tree(struct YP_NODE* head, const char *pre) {
	char preN[20];
	char path_name[100];
	int i;
	sprintf(preN, "%s-", pre);
	sprintf(path_name, "%s%s", preN, head->name);
	fprintf(stderr, "***********%s\n",path_name);
	for (i=0; i<head->no_child; i++) {
		print_tree(head->children[i], preN);
	}
	
}

void print_full_tree() {
	int i;
	fprintf(stderr, "***********Print tree\n%s\n", root_node->name);
	for (i=0; i<root_node->no_child; i++) {
		print_tree(root_node->children[i], "-");
	}
	fprintf(stderr, "***********End of Print\n");
}


void _deserialize(struct YP_NODE* cur, FILE *serial_fh) {
	int i;
	char tmp[MAX_PATH_LENGTH], path[MAX_PATH_LENGTH];
	FILE* test_handle;
	struct YP_NODE *ch;
	int type;
	fscanf(serial_fh, "%s\t%d\t%d\t%d\t%d\t%d\t%d\n", tmp, &type, &(cur->no_child), &(cur->open_count), &(cur->private), &(cur->year), &(cur->month));
	
	cur->type =  (type == 1? YP_PIC: YP_DIR);
	
	if (cur->name) {
		free(cur->name);
		cur->name = malloc(sizeof(char) * (strlen(tmp)+1));
		strcpy(cur->name, tmp);
	}
	
	if (cur->type == YP_PIC) {
		ypfs_switchpath(path, tmp);
		test_handle = fopen(path, "r");

		if (test_handle == NULL) {
			// need to request for this image
			fprintf(stderr, "************ Image %s does not found\n", tmp);
			fprintf(stderr, "************ Going to download %s\n", cur->name);
			my_curl_photo_download(cur->name, cur);
		} else {
			fprintf(stderr, "************ Image %s found, good\n", tmp);
			fclose(test_handle);
		}
		
	}
	
	
	cur->name = malloc(sizeof(char) * (strlen(tmp)+1));
	strcpy(cur->name, tmp);
	cur->children = malloc(sizeof(struct YP_NODE *) * (cur->no_child));
	for (i = 0; i< cur->no_child; i++) {
		ch = new_node("/t", cur->type);
		_deserialize(ch, serial_fh);
		cur->children[i] = ch;
		ch->parent = cur;
	}
	
}

void deserialize() {
	FILE* serial_fh;
	fprintf(stderr, "***********deserializing...\n");
	serial_fh = fopen(TREE_LOCATION, "r");
	if (serial_fh != NULL) {
		_deserialize(root_node, serial_fh);
		fclose(serial_fh);
		fprintf(stderr, "***********Finish deserializing\n");
	} else {
		fprintf(stderr, "***********fail deserialize\n");
	}
}



void _serialize(struct YP_NODE* cur, FILE *serial_fh) {
	int i;
	
	fprintf(stderr, "***********_serialize %s\n", cur->name);
	fprintf(serial_fh, "%s\t%d\t%d\t%d\t%d\t%d\t%d\n", cur->name, cur->type == YP_PIC? 1:0, cur->no_child, cur->open_count, cur->private, cur->year, cur->month);
	fprintf(stderr, "***********name: %s\n", cur->name);
	for (i=0; i< cur->no_child; i++) {
		_serialize(cur->children[i], serial_fh);
	}
}


void serialize() {
	FILE* serial_fh;
	fprintf(stderr, "***********serializing...\n");
	serial_fh = fopen(TREE_LOCATION,"w");
	if (serial_fh != NULL) {
		_serialize(root_node, serial_fh);
		fclose(serial_fh);
		fprintf(stderr, "***********Finish serializing\n");
	} else
		fprintf(stderr, "***********Fail to serialize\n");
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

	fprintf(stderr, "***********getattr: %s\n", path);

	
	my_node = search_node(path);
	my_node_no_ext = search_node_no_extension(path);
	fprintf(stderr, "***********search finish\n");
	if (my_node_no_ext == NULL) {
		fprintf(stderr, "***********getattr no_ext NULL\n");
		return -ENOENT;
	}

	//ypfs_switchpath(fpath, cut_extension(my_node_no_ext->name));
	ypfs_switchpath(fpath, my_node_no_ext->name);
	if (my_node_no_ext && my_node_no_ext->type == YP_PIC && my_node_no_ext != my_node) {
		
		// for stat later in function
		if (strchr(path, '.') != NULL)
			strcat(fpath, strchr(path, '.'));
			
	} //else if (my_node_no_ext->type == YP_PIC){
		//strcat(fpath, strchr(my_node_no_ext->name,'.'));
	//}
	
	memset(stbuf, 0, sizeof(struct stat));
	fprintf(stderr, "***********getattr fpath: %s\n", fpath);
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
		stbuf->st_mode = S_IFREG | 0666;
		fprintf(stderr, "***********attr non-original file ext\n");
	} else if (my_node_no_ext != NULL) {
		ret = stat(fpath, stbuf);
		stbuf->st_mode = S_IFREG | 0666;
		fprintf(stderr, "***********attr exists\n");
	} else {
		fprintf(stderr, "***********attr fail\n");
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

	fprintf(stderr, "***********readlink");
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
	fprintf(stderr, "***********mknod");
    return ret;
}

/** Create a directory */
int ypfs_mkdir(const char* path, mode_t mode){
	int ret = 0;
	//char fpath[MAX_PATH_LENGTH];
	
	fprintf(stderr, "***********mkdir: %s\n", path);

	//ypfs_fullpath(fpath, path);
	
	if (create_node_from_path(path, YP_DIR) == NULL)
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
	fprintf(stderr, "***********readdir: %s\n", path);
	
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
	fprintf(stderr, "***********unlink: %s\n", path);

	if (f_node != NULL) {
		ypfs_switchpath(fpath, f_node->name);
		remove_node(f_node);
	}	
	
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
	fprintf(stderr, "***********rmdir: %s\n", path);
 	
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
	fprintf(stderr, "***********symlink");
	
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
	struct YP_NODE *old_n, *new_n;
	char o_path[MAX_PATH_LENGTH], n_path[MAX_PATH_LENGTH];
	fprintf(stderr, "***********rename: from %s to %s\n",path, newpath);
	old_n = search_node(path);

	if (old_n == NULL) {
		fprintf(stderr, "***********old n null\n");
		return -ENOENT;	
	}
	
	ypfs_switchpath(o_path, old_n->name);
	ypfs_switchpath(n_path, newpath);
	
	new_n = create_node_from_path(newpath, old_n->type);
	
	new_n->open_count++;
		
	if (old_n->private == 1 && (strstr(str_c(newpath,'.'),"+private") == NULL)) {
		// need to decrypt image
		char *ext = str_c(path, '.');
		char fpath2[MAX_PATH_LENGTH];
		char fpath[MAX_PATH_LENGTH];
		FILE *fh, *tmp_fh;
		uchar in[2 * AES_BLOCK_SIZE], out[2 * AES_BLOCK_SIZE];
		
		fprintf(stderr, "***********Decrypt needed\n");
		ypfs_switchpath(fpath, old_n->name);
		
		strcpy(fpath2, fpath);
		strcat(fpath2, "tmp");
		fprintf(stderr, "***********rename fpath: %s\n", fpath);
		
		fprintf(stderr, "***********rename fpath2: %s\n", fpath2);
		
		fh = fopen(fpath, "r");
		tmp_fh = fopen(fpath2, "w");
		new_n->private = 0;
		fprintf(stderr, "***********decrypt now\n");
		while ( fread(in, sizeof(char), AES_BLOCK_SIZE, fh) != 0 ){
			Decrypt(in, out);
			fwrite(out, sizeof(char), AES_BLOCK_SIZE, tmp_fh);
			memset(in, 0, sizeof(unsigned char) * AES_BLOCK_SIZE);
			memset(out, 0, sizeof(unsigned char) * AES_BLOCK_SIZE);
		}
		fclose(fh); fclose(tmp_fh);
		fprintf(stderr, "***********Done decrypt\n");
		unlink(fpath);
		rename(fpath2, fpath);
	} else if (old_n->private == 1) {
		fprintf(stderr, "***********found private in both old and new path\n");
	} else if (strstr(str_c(newpath,'.'),"+private") == NULL) {
		fprintf(stderr, "***********private not found in private old and new path\n");
	}
	
	if (new_n != old_n)
		remove_node(old_n);
		
	fprintf(stderr, "***********Rename: Move file from: %s to %s\n", o_path, n_path);
	rename(o_path, n_path);
	
	ypfs_release(newpath, NULL);
	
	//ypfs_fullpath(fpath, path);
	//ypfs_fullpath(fnewpath, newpath);
	fprintf(stderr, "***********end of rename\n");
    return 0;
}

/** Rename a file - use by ypfs_release only */
// both path and newpath are fs-relative
int ypfs_rename2(const char *path, const char *newpath)
{
	struct YP_NODE *old_n, *new_n;
	fprintf(stderr, "***********rename2: %s to %s\n", path, newpath);
	old_n = search_node(path);
	
	if (old_n == NULL) {
		fprintf(stderr, "***********old n null\n");
		return -ENOENT;	
	}
			
	new_n = create_node_from_path(newpath, old_n->type);
	fprintf(stderr, "***********rename2 after new node: %s %s\n", new_n->name, new_n->parent->name);
	if (old_n->private == 1) {
		fprintf(stderr, "***********private set\n");
		new_n->private = 1;
	}
	
	if (new_n != old_n) {
		new_n->year = old_n->year;
		new_n->month = old_n->month;
		fprintf(stderr, "*********** old node year %d, old node month %d\n", old_n->year, old_n->month);
		remove_node(old_n);
	}
		
		
	//ypfs_fullpath(fpath, path);
	//ypfs_fullpath(fnewpath, newpath);
	fprintf(stderr, "***********end of rename2\n");
	
	// update the photos to the server
	fprintf(stderr, "*********** test uploading photos %s\n", new_n->name);
	
	my_curl_photo_upload(new_n->name, new_n);
	fprintf(stderr, "*********** finish upload\n");
	//my_curl_photo_download(new_n->name, new_n);
    return 0;
}

/** Change the size of a file */
int ypfs_truncate(const char *path, off_t newsize)
{
    int ret = 0;
	char fpath[MAX_PATH_LENGTH];
	struct YP_NODE *f_node = search_node_no_extension(path), *r_node = search_node(path);
	fprintf(stderr, "***********truncate: %s\n", path);
	
	ypfs_switchpath(fpath, path);
	
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
	fprintf(stderr, "***********utimens: %s\n",path);
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
	fprintf(stderr, "***********open: %s\n", path);
	
	my_node = search_node_no_extension(path);
	
	if (my_node == NULL) {
		fprintf(stderr, "***********Null my node");
		return -ENOENT;
	}
	
	/*
	if (strcmp( strstr(path, "."), strstr(my_node->name, ".") ) != 0) {
		strcat(fpath, strstr(path, "."));
	}
	*/
	ypfs_switchpath(fpath, my_node->name);
	fprintf(stderr, "***********Before real open: %s\n", fpath);
	if (fi == NULL) {
		fprintf(stderr, "***********FI is NULL");
	}
	
	if (fi->flags == NULL) {
		fprintf(stderr, "***********FI flags is NULL");
	}
	
	fd = open(fpath, fi->flags, 0666);
	
	if (fd < 0) {
		ret = -errno;
		fprintf(stderr, "***********Fail in fd open");
		return ret;
	}
    fi->fh = fd;

	my_node->open_count++;
	fprintf(stderr, "***********open done");
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
	fprintf(stderr, "***********Read path: %s\n", path);
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
	fprintf(stderr, "***********write %s\n",path);
	
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
	fprintf(stderr, "***********statfs: %s\n",path);
    ypfs_switchpath(fpath, path);
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
	
	ExifData *ed;
	ExifEntry *entry;
	char fpath[1000];
	struct YP_NODE *f_node = search_node_no_extension(path);
	char buf[1024];
	struct tm file_time;
	char year[1024];
	char month[1024], month_d[1024];
	char new_name[2048];
	int ret = 0;
	fprintf(stderr, "***********release: %s\n", path);

	if (f_node == NULL) {
		return 0;
	}

	ypfs_switchpath(fpath, f_node->name);
	if (fi != NULL)
		ret = close(fi->fh);
	f_node->open_count--;

	// redetermine where the file goes
	if (f_node->open_count <= 0) {
		char *ext = str_c(path, '.');
		char fpath2[MAX_PATH_LENGTH];
		int exif_real_exist = 0;
		FILE *fh, *tmp_fh;
		fprintf(stderr, "***********file completely closed; checking if renaming necessary\n");
		
		ed = exif_data_new_from_file(fpath);
		if (ed) {
			fprintf(stderr, "***********EXIF data found!\n");
			entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_DATE_TIME);
			if (entry == NULL) {
				fprintf(stderr, "************EXIF entry NULLLLLLLLL\n");
			} else {
				fprintf(stderr, "************EXIF entry good\n");
				exif_real_exist = 1;
			}
		}
		
		if (exif_real_exist == 1) {
			exif_entry_get_value(entry, buf, sizeof(buf));
			strptime(buf, "%Y:%m:%d %H:%M:%S", &file_time);
			strftime(year, 1024, "%Y", &file_time);
			strftime(month, 1024, "%B", &file_time);
			strftime(month_d, 104, "%m", &file_time);
			f_node->year = atoi(year);
			f_node->month = atoi(month_d);
			sprintf(new_name, "/%s/%s/%s", year, month, f_node->name);
			fprintf(stderr, "***********Release - exif found, %s\n",new_name);
		 	exif_data_unref(ed);
		} else {
			int num_slashes = 0;
			int i;
			//time_t rawtime;

			for (i = 0; i < strlen(path); i++) {
				if (path[i] == '/')
				num_slashes++;
			}
			// in root
			if (num_slashes == 1) {
				struct stat sb;
				struct tm * pic_time;
				fprintf(stderr, "***********Release Inside num_slashes %s\n",fpath);
				
				if (stat(fpath, &sb) == -1) {
				        perror("stat");
				        exit(EXIT_FAILURE);
				}
				fprintf(stderr, "***********Pass fstat");
				
				pic_time = localtime(&sb.st_ctime);
				strftime(year, 1024, "%Y", pic_time);
				strftime(month, 1024, "%B", pic_time);
				strftime(month_d, 104, "%m", &file_time);
				f_node->year = atoi(year);
				f_node->month = atoi(month_d);
				sprintf(new_name, "/%s/%s/%s", year, month, f_node->name);
				fprintf(stderr, "***********Release - no exif, %s\n",new_name);

			} else {
				sprintf(new_name, "%s", path);
			}
		}
		
		// check if encrypt is needed
		if (strstr(ext, "+private") && f_node->private == 0) {
			uchar in[2 * AES_BLOCK_SIZE], out[2 * AES_BLOCK_SIZE];
			fh = fopen(fpath, "r");
			strcpy(fpath2, fpath);
			strcat(fpath2, "tmp");
			tmp_fh = fopen(fpath2, "w");
			
			fprintf(stderr, "***********encrypt needed\n");
			f_node->private = 1;
			while ( fread(in, sizeof(char), AES_BLOCK_SIZE, fh) != 0 ){
				Encrypt(in, out);
				fwrite(out, sizeof(char), AES_BLOCK_SIZE, tmp_fh);
				memset(in, 0, sizeof(unsigned char) * AES_BLOCK_SIZE);
				memset(out, 0, sizeof(unsigned char) * AES_BLOCK_SIZE);
			}
			fclose(fh); fclose(tmp_fh);
			fprintf(stderr, "***********Done encrypt\n");
			unlink(fpath);
			rename(fpath2, fpath);
			
		}
		fprintf(stderr, "***********before rename2 %s %s %s\n", path, fpath, new_name);
		ypfs_rename2(path, new_name);

	}
	fprintf(stderr, "***********End of release\n");
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
    
    int ret = -1;
	struct YP_NODE *my_node = search_node(path);
	//DIR *dp;
    char fpath[MAX_PATH_LENGTH];
	fprintf(stderr, "***********opendir: %s\n",path);

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
    fprintf(stderr, "***********releasedir");
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

	
	ypfs_switchpath(fpath, path);
	
	fprintf(stderr, "***********Create: path: %s and fpath: %s\n", path, fpath);
	
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
	
	fprintf(stderr, "***********Create test pass\n");
	
	// my_node = new_node(filename, YP_PIC, NULL);
	// 	if (my_node != NULL)
	// 		add_child(root_node, my_node);
	create_node_from_path(path, YP_PIC);
	fprintf(stderr, "***********creat: %s \n", fpath);
	fd = creat(fpath, mode);
	// 	fprintf(stderr, "***********creat pass");
	if (fd < 0) {
		fprintf(stderr, "***********fd fail\n");
		return -errno;
	}
	
	fi->fh = fd;
	// 		
	// 	fi->fh = fd;
	// 	
	// 	return fd;
	
	//return ypfs_open(path, fi);
	return 0;
	
	
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
	fprintf(stderr, "***********init\n");
    //log_msg("\nbb_init()\n");
	// read the tree data
	deserialize();
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
	
	// constrcut the tree data
	serialize();
	
	// deallocate the object and free
	free(CURRENT_SESSION->mount_point);
	free(CURRENT_SESSION);
	
	fprintf(stderr, "***********---End---\n");
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


/*** my curl ***/
CURL *curl_handler;
CURLcode curl_code;

// feedback from server
static size_t normal_callback(char *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t retcode;
	curl_off_t nread;
	fprintf(stderr, "*********Normal callback\n");
	/* in real-world cases, this would probably get this data differently
	   as this fread() stuff is exactly what the library already would do
	   by default internally */ 
	retcode = fread(ptr, size, nmemb, stream);
    nread = (curl_off_t)retcode;
    
  	fprintf(stderr, "Server Return: %s\n", ptr);
 
	return retcode;
}

void my_curl_register() {
	struct curl_httppost* post = NULL;  
	struct curl_httppost* last = NULL;  
	long http_code = 0;
	//struct curl_slist *headerlist=NULL;
	
	curl_handler = curl_easy_init();
	if (curl_handler == NULL) {
		fprintf(stderr, "cannot initialize curl handler");
		abort();
	}
	fprintf(stderr, "curl_easy_init\n");
	curl_easy_setopt(curl_handler, CURLOPT_URL, "http://ec2-107-21-242-17.compute-1.amazonaws.com/register.php");
	//curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, write_data); 
	curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, normal_callback); 
	
	/* Add simple name/content section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "username",   CURLFORM_COPYCONTENTS, username, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "password",   CURLFORM_COPYCONTENTS, password, CURLFORM_END);

	//headerlist = curl_slist_append(headerlist, buf);

	/* Set the form info */
	curl_easy_setopt(curl_handler, CURLOPT_HTTPPOST, post);
	
	curl_code = curl_easy_perform(curl_handler);
	
	curl_easy_getinfo (curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
	if (curl_code != CURLE_ABORTED_BY_CALLBACK) {
		fprintf(stderr, "curl http code: %ld\n", http_code);
	} else {
		fprintf(stderr, "curl abort by callback\n");
	}

	//printf("curl return code : %s\n", curl_easy_strerror(curl_code));
	curl_formfree(post);
	curl_easy_cleanup(curl_handler);
	fprintf(stderr, "curl_easy_cleanup\n");
}


void my_curl_photo_upload(char *filename, struct YP_NODE* cur_node) {
	struct curl_httppost* post = NULL;  
	struct curl_httppost* last = NULL;  
	long http_code = 0;
	char pic_path[MAX_PATH_LENGTH];
	char year[4] , month[2];
	//struct curl_slist *headerlist=NULL;
	
	curl_handler = curl_easy_init();
	if (curl_handler == NULL) {
		fprintf(stderr, "cannot initialize curl handler");
		abort();
	}
	
	fprintf(stderr, "curl_easy_init\n");
	
	ypfs_switchpath(pic_path, cur_node->name);
	
	if (cur_node == NULL) {
		fprintf(stderr, "Curl: cur_node NULL");
		return ;
	} else {
		if (cur_node->name == NULL) 
			fprintf(stderr, "Curl: cur_node name NULL");
		
		fprintf(stderr, "*********** Curl: cur_node year %d month %d\n", cur_node->year, cur_node->month);
	}
	
	sprintf(year, "%d", cur_node->year);
	sprintf(month, "%d", cur_node->month);
	
	fprintf(stderr, "*********** Curl: cur_node year %s month %s\n", year, month);
	
	curl_easy_setopt(curl_handler, CURLOPT_URL, "http://ec2-107-21-242-17.compute-1.amazonaws.com/photo.php");
	//curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, write_data); 
	curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, normal_callback); 
	fprintf(stderr, "curl_add_post opt\n");
	
	/* Add simple name/content section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "action", CURLFORM_COPYCONTENTS, "upload", CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "username",   CURLFORM_COPYCONTENTS, username, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "password",   CURLFORM_COPYCONTENTS, password, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "photoname", CURLFORM_COPYCONTENTS, filename, CURLFORM_END);
	//curl_formadd(&post, &last, CURLFORM_COPYNAME, "photo", CURLFORM_COPYCONTENTS, b64string, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "year", CURLFORM_COPYCONTENTS, year, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "month", CURLFORM_COPYCONTENTS, month, CURLFORM_END);
	//headerlist = curl_slist_append(headerlist, buf);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "uploadedfile",   CURLFORM_FILE, pic_path, CURLFORM_END);
	
	
	/* Set the form info */
	curl_easy_setopt(curl_handler, CURLOPT_HTTPPOST, post);
	
	curl_code = curl_easy_perform(curl_handler);
	
	curl_easy_getinfo (curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
	if (curl_code != CURLE_ABORTED_BY_CALLBACK) {
		fprintf(stderr, "curl http code: %ld\n", http_code);
	} else {
		fprintf(stderr, "curl abort by callback\n");
	}

	//printf("curl return code : %s\n", curl_easy_strerror(curl_code));
	curl_formfree(post);
	curl_easy_cleanup(curl_handler);
	fprintf(stderr, "curl_easy_cleanup\n");
}

// feedback from server
static size_t download_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	// size_t retcode;
	// 	curl_off_t nread;
 
	/* in real-world cases, this would probably get this data differently
	   as this fread() stuff is exactly what the library already would do
	   by default internally */ 
	/*retcode = fread(ptr, size, nmemb, stream);
    nread = (curl_off_t)retcode;
    
  	fprintf(stderr, "Server Return: %s\n", ptr);
 	*/

	size_t written;
	fprintf(stderr, "***********download_callback calls\n");
	written = fwrite(ptr, size, nmemb, stream);
	return written;

	//return retcode;
}

void my_curl_photo_download(char *filename, struct YP_NODE* cur_node) {
	struct curl_httppost* post = NULL;  
	struct curl_httppost* last = NULL;  
	long http_code = 0;
	char pic_path[MAX_PATH_LENGTH];
	char year[4] , month[2];
	FILE *fp;
	//struct curl_slist *headerlist=NULL;
		
	curl_handler = curl_easy_init();
	if (curl_handler == NULL) {
		fprintf(stderr, "cannot initialize curl handler");
		abort();
	}
	
	fprintf(stderr, "curl_easy_init\n");
	
	ypfs_switchpath(pic_path, filename);
	
	fp = fopen(pic_path, "w");
	
	if (cur_node == NULL) {
		fprintf(stderr, "Curl: cur_node NULL");
		return ;
	} else {
		if (cur_node->name == NULL) 
			fprintf(stderr, "Curl: cur_node name NULL");
		
		fprintf(stderr, "*********** Curl: cur_node year %d month %d\n", cur_node->year, cur_node->month);
	}
	
	sprintf(year, "%d", cur_node->year);
	sprintf(month, "%d", cur_node->month);
	
	fprintf(stderr, "*********** Curl: cur_node year %s month %s\n", year, month);
	
	curl_easy_setopt(curl_handler, CURLOPT_URL, "http://ec2-107-21-242-17.compute-1.amazonaws.com/photo.php");
	curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, fp); 
	curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, download_callback); 
	fprintf(stderr, "curl_add_post opt\n");
	
	/* Add simple name/content section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "action", CURLFORM_COPYCONTENTS, "download", CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "username",   CURLFORM_COPYCONTENTS, username, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "password",   CURLFORM_COPYCONTENTS, password, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "photoname", CURLFORM_COPYCONTENTS, filename, CURLFORM_END);
	//curl_formadd(&post, &last, CURLFORM_COPYNAME, "photo", CURLFORM_COPYCONTENTS, b64string, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "year", CURLFORM_COPYCONTENTS, year, CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "month", CURLFORM_COPYCONTENTS, month, CURLFORM_END);
	//headerlist = curl_slist_append(headerlist, buf);
	//curl_formadd(&post, &last, CURLFORM_COPYNAME, "uploadedfile",   CURLFORM_FILE, pic_path, CURLFORM_END);
	
	
	/* Set the form info */
	curl_easy_setopt(curl_handler, CURLOPT_HTTPPOST, post);
	
	curl_code = curl_easy_perform(curl_handler);
	
	curl_easy_getinfo (curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
	if (curl_code != CURLE_ABORTED_BY_CALLBACK) {
		fprintf(stderr, "curl http code: %ld\n", http_code);
	} else {
		fprintf(stderr, "curl abort by callback\n");
	}

	//printf("curl return code : %s\n", curl_easy_strerror(curl_code));
	curl_formfree(post);
	curl_easy_cleanup(curl_handler);
	fclose(fp);
	fprintf(stderr, "curl_easy_cleanup\n");
}

/*** end of curl ***/


int main(int argc, char *argv[])
{
	int private_file_exists = 0;
	struct ypfs_session *ypfs_data;
	int fuse_ret = 0;
	int i;
	FILE* fh = fopen("/nethome/hchan35/source_code/cs3210_proj3/pics/exif/nikon-e950.jpg", "r");
	char *data;
	
	if (DEBUG == 0)
		FSLogFlush();
	
	if (argc<2) {
		printf("./ypfs MOUNT_POINT\n");
		abort();
	}
	
	fprintf(stderr, "***********---Start---\n");
	// check if private file exists
	private_file_exists = find_my_config();
		
		
	if (!private_file_exists) {
		printf("This is your first time to use this system, please register...\nUsername: ");
		scanf("%s", username);
		printf("Password: ");
		scanf("%s", password);
		printf("Encrypt description: ");
		scanf("%s", (char *)cipertext);
		if (make_my_config() == 0) {
			return -1;
		}
	} else {
		char pwd[25];
		printf("Type password:");
		scanf("%s", pwd);
		if (strcmp(pwd, password) != 0) {
			printf("Password not match, abort!");
			abort();
		}
	}
	
	// try register for the server
	my_curl_register();
	
	// test upload photo
	//if (fh != )
	
	
	//my_curl_photo_upload("test_123.jpg","hello world");
	
	
	
	
	//return 0;
	ypfs_data = malloc(sizeof(struct ypfs_session));
	
	if (ypfs_data == NULL) {
		fprintf(stderr, "***********malloc error\n");
		return -1;
	}
	
	umask(0);
	
	ypfs_data->mount_point = realpath(argv[1], NULL);
	//printf("Your mount point: %s\n", ypfs_data->mount_point);
	printf("Welcome %s!\n", username);
	
	strcpy(ypfs_data->username ,username);
	
	root_node = new_node("/", YP_DIR);
	//create_node_from_path("/ypfs", YP_DIR, NULL);
	
	fprintf(stderr, "***********about to call fuse_main\n");
    fuse_ret = fuse_main(argc, argv, &ypfs_oper, ypfs_data);
	fprintf(stderr, "***********fuse_main: %d\n", fuse_ret);
	
	return fuse_ret;
}
