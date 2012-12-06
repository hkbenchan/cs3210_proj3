#ifndef YPFS_CURL_H
#define YPFS_CURL_H
 
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream);
int ypfs_curl(char *file, char *url);
 
#endif
