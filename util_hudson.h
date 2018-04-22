#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include "pipe/pipe.h"
#include "pipe/pipe_util.h"
#include <json-c/json.h>
#include "gumbo.h"
#include <curl/curl.h>
#include "regex-c/re.h"
#include "libpqueue/src/pqueue.h"
#include "hashset/hashset.h"

typedef char* string;

bool strcontain(string body, string request){
	if (strstr(body, request) != NULL) {
		return true;
	}
	return false;
}

long int hex_to_int(const char * input){
    return strtol(input + 2, (char **)NULL, 16);
}

long int hex_to_int_no_x(const char * input){
    return strtol(input, (char **)NULL, 16);
}

int backpedal_to_char(const char * text, const char query, int starting_index){
    int i = starting_index;
    for(; i > -1; i--){
        if ( text[i] == query ) return i;
    }
    return -1;
}

int look_for_to_char(const char * text, const char query, int starting_index){
    int i = starting_index;
    for(; i < strlen(text); i++){
        if ( text[i] == query ) return i;
    }
    return -1;
}


static char hex [] = { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9' ,'A', 'B', 'C', 'D', 'E', 'F' };
 
string dec_to_hex(int dec, string buff){
    strcat(buff, "0x");
    sprintf(buff+2, "%X", dec);
    return buff;
}

void swap(long int *xp, long int *yp)
{
    long int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

long int string_to_epoch(string input){
	long int output = 0;
	string ptr;
	FILE *fp;
	char command[256];
	strcpy(command, "/bin/date -d \'");
	strcat(command, input);
	strcat(command, "\' \"+%s\"");
	char path[64];
	fp = popen( command, "r" );
	while (fgets(path, sizeof(path)-1, fp) != NULL) {
		if(strcontain(path, "invalid")){
			pclose(fp);
			exit(1);
		}
		pclose(fp);
		return strtol(path, &ptr, 10);
	}
	pclose(fp);
	exit(1);
}

/* holder for curl fetch */
struct curl_fetch_st {
    char *payload;
    size_t size;
};

/* callback for curl fetch */
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                             /* calculate buffer size */
    struct curl_fetch_st *p = (struct curl_fetch_st *) userp;   /* cast pointer to fetch struct */

    /* expand buffer */
    p->payload = (char *) realloc(p->payload, p->size + realsize + 1);

    /* check buffer */
    if (p->payload == NULL) {
      /* this isn't good */
      fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
      /* free buffer */
      free(p->payload);
      /* return */
      return -1;
    }

    /* copy contents to buffer */
    memcpy(&(p->payload[p->size]), contents, realsize);

    /* set new buffer size */
    p->size += realsize;

    /* ensure null termination */
    p->payload[p->size] = 0;

    /* return size */
    return realsize;
}

struct json_object * find_something(struct json_object *jobj, const char *key) {
    struct json_object *tmp;

    json_object_object_get_ex(jobj, key, &tmp);

    return tmp;
}

/* fetch and return url body via curl */
CURLcode curl_fetch_url(CURL *ch, const char *url, struct curl_fetch_st *fetch) {
    CURLcode rcode;                   /* curl result code */

    /* init payload */
    fetch->payload = (char *) calloc(1, sizeof(fetch->payload));

    /* check payload */
    if (fetch->payload == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to allocate payload in curl_fetch_url");
        /* return error */
        return CURLE_FAILED_INIT;
    }

    /* init size */
    fetch->size = 0;

    /* set url to fetch */
    curl_easy_setopt(ch, CURLOPT_URL, url);

    /* set calback function */
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);

    /* pass fetch struct pointer */
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) fetch);

    /* set default user agent */
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* set timeout */
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, 5);

    /* enable location redirects */
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

    /* set maximum allowed redirects */
    curl_easy_setopt(ch, CURLOPT_MAXREDIRS, 1);

    /* fetch the url */
    rcode = curl_easy_perform(ch);

    /* return */
    return rcode;
}

json_object* get_json_from_post(string url, json_object *json){
	CURL *ch;                                               /* curl handle */
    CURLcode rcode;                                         /* curl result code */

    enum json_tokener_error jerr = json_tokener_success;    /* json parse error */

    struct curl_fetch_st curl_fetch;                        /* curl fetch struct */
    struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
    struct curl_slist *headers = NULL;                      /* http headers to send with request */

    if ((ch = curl_easy_init()) == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");
        /* return error */
        exit(1);
        //return 1;
    }

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    

    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ch, CURLOPT_POSTFIELDS, json_object_to_json_string(json));

    rcode = curl_fetch_url(ch, url, cf);

    /* cleanup curl handle */
    curl_easy_cleanup(ch);

    /* free headers */
    curl_slist_free_all(headers);

    /* free json object */
    json_object_put(json);

    /* check return code */
    if (rcode != CURLE_OK || cf->size < 1) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s\n",
            url, curl_easy_strerror(rcode));
        /* return error */
        exit(1);
        //return 2;
    }
    /* check payload */
    if (cf->payload != NULL) {
        /* parse return */
        json = json_tokener_parse_verbose(cf->payload, &jerr);
        /* free payload */
        free(cf->payload);

        
    } else {
        /* error */
        fprintf(stderr, "ERROR: Failed to populate payload\n");
        /* free payload */
        free(cf->payload);
        /* return */
        exit(1);
        //return 3;
    }

    /* check error */
    if (jerr != json_tokener_success) {
        /* error */
        fprintf(stderr, "ERROR: Failed to parse json string\n");
        /* free json object */
        json_object_put(json);
        /* return */
        exit(1);
        //return 4;
    }
    return json;
}

struct curl_fetch_st* get_html_from_post(string url, json_object* json){
    if (json == NULL) json = json_object_new_object();
    CURL *ch;                                               /* curl handle */
    CURLcode rcode;                                         /* curl result code */

    struct curl_fetch_st curl_fetch;                        /* curl fetch struct */
    struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
    struct curl_slist *headers = NULL;                      /* http headers to send with request */

    if ((ch = curl_easy_init()) == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");
        /* return error */
        exit(1);
        //return 1;
    }

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ch, CURLOPT_POSTFIELDS, json_object_to_json_string(json));

    rcode = curl_fetch_url(ch, strstr(url, "http"), cf);

    /* cleanup curl handle */
    curl_easy_cleanup(ch);

    /* free headers */
    curl_slist_free_all(headers);

    /* free json object */
    json_object_put(json);

    /* check return code */
    if (rcode != CURLE_OK || cf->size < 1) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s\n",
            url, curl_easy_strerror(rcode));
        /* return error */
        exit(1);
        //return 2;
    }
    /* check payload */
    if (cf->payload != NULL) {
        /* parse return */
        return cf;
        /* free payload */
        free(cf->payload);
    } else {
        /* error */
        fprintf(stderr, "ERROR: Failed to populate payload");
        /* free payload */
        free(cf->payload);
        /* return */
        exit(1);
        //return 3;
    }

    return cf;
}