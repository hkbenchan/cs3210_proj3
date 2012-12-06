#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
 
#include <curl/curl.h>

#include "ypfs_curl.h"
 
/*
 * This example shows a HTTP PUT operation. PUTs a file given as a command
 * line argument to the URL also given on the command line.
 *
 * This example also uses its own read callback.
 *
 * Here's an article on how to setup a PUT handler for Apache:
 * http://www.apacheweek.com/features/put
 */ 
 
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t retcode;
  curl_off_t nread;
 
  /* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */ 
  retcode = fread(ptr, size, nmemb, stream);
 
  nread = (curl_off_t)retcode;
 
  fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T
          " bytes from file\n", nread);
 
  return retcode;
}
 
int ypfs_curl(char *file, char *url)
{
  CURL *curl;
  CURLcode res;
  FILE * hd_src ;
  int hd ;
  struct stat file_info;
 
  /* get the file size of the local file */ 
  hd = open(file, O_RDONLY) ;
  fstat(hd, &file_info);
  close(hd) ;
 
  /* get a FILE * of the same file, could also be made with
     fdopen() from the previous descriptor, but hey this is just
     an example! */ 
  hd_src = fopen(file, "rb");
 
  /* In windows, this will init the winsock stuff */ 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* get a curl handle */ 
  curl = curl_easy_init();
  if(curl) {
    /* we want to use our own read function */ 
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
 
    /* enable uploading */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
    /* HTTP PUT please */ 
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
 
    /* specify target URL, and note that this URL should include a file
       name, not only a directory */ 
    curl_easy_setopt(curl, CURLOPT_URL, url);
 
    /* now specify which file to upload */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);
 
    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);
 
    /* Now run off and do what you've been toldx! */ 
    res = curl_easy_perform(curl);
    /* Check for errors */ 
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
 
    /* always cleanup */ 
    curl_easy_cleanup(curl);
  }
  fclose(hd_src); /* close the local file */ 
 
  curl_global_cleanup();
  return 0;
}



void my_fail_method() {
	
	struct curl_httppost* post = NULL;  
	struct curl_httppost* last = NULL;  
	char namebuffer[] = "name buffer";  
	long namelength = strlen(namebuffer);  
	char buffer[] = "test buffer";  
	char htmlbuffer[] = "<HTML>test buffer</HTML>";  
	long htmlbufferlength = strlen(htmlbuffer);  
	struct curl_forms forms[3];  
	char file1[] = "my-face.jpg";  
	char file2[] = "your-face.jpg";  
	/* add null character into htmlbuffer, to demonstrate that transfers of buffers containing null characters actually work  */  
	htmlbuffer[8] = '\0';

	/* Add simple name/content section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "name",   CURLFORM_COPYCONTENTS, "content", CURLFORM_END);

	/* Add simple name/content/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "htmlcode", CURLFORM_COPYCONTENTS, "<HTML></HTML>", CURLFORM_CONTENTTYPE, "text/html", CURLFORM_END);

	/* Add name/ptrcontent section */  
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "name_for_ptrcontent",   CURLFORM_PTRCONTENTS, buffer, CURLFORM_END);

	/* Add ptrname/ptrcontent section */
	curl_formadd(&post, &last, CURLFORM_PTRNAME, namebuffer,   CURLFORM_PTRCONTENTS, buffer, CURLFORM_NAMELENGTH,   namelength, CURLFORM_END);

	/* Add name/ptrcontent/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "html_code_with_hole",   CURLFORM_PTRCONTENTS, htmlbuffer,   CURLFORM_CONTENTSLENGTH, htmlbufferlength,   CURLFORM_CONTENTTYPE, "text/html", CURLFORM_END);

	/* Add simple file section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "picture",   CURLFORM_FILE, "my-face.jpg", CURLFORM_END);

	/* Add file/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "picture",   CURLFORM_FILE, "my-face.jpg",   CURLFORM_CONTENTTYPE, "image/jpeg", CURLFORM_END);

	/* Add two file section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "pictures",   CURLFORM_FILE, "my-face.jpg",   CURLFORM_FILE, "your-face.jpg", CURLFORM_END);

	/* Add two file section using CURLFORM_ARRAY */
	forms[0].option = CURLFORM_FILE;  forms[0].value = file1;  forms[1].option = CURLFORM_FILE;  forms[1].value = file2;  forms[2].option = CURLFORM_END;

	/* Add a buffer to upload */
	curl_formadd(&post, &last,   CURLFORM_COPYNAME, "name",   CURLFORM_BUFFER, "data",   CURLFORM_BUFFERPTR, record,   CURLFORM_BUFFERLENGTH, record_length,   CURLFORM_END);

	/* no option needed for the end marker */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "pictures",   CURLFORM_ARRAY, forms, CURLFORM_END);
	
	/* Add the content of a file as a normal post text value */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "filecontent",   CURLFORM_FILECONTENT, ".bashrc", CURLFORM_END);
	
	/* Set the form info */ 
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	
}