#ifndef _H_HTML
#define _H_HTML

#include <libmill.h>
#define NOB_IMPLEMENTATION
#include "nob.h"


int is_inside_folder(const char *file_path, const char *folder_path){
    char *file = realpath(file_path, NULL);
    char *folder = realpath(folder_path, NULL);

    if (!file || !folder) {
        free(file);
        free(folder);
        return 0;
    }

    size_t len = strlen(folder);

    int result = strncmp(file, folder, len) == 0 &&
                 (file[len] == '/' || file[len] == '\0');

    free(file);
    free(folder);
    return result;
}


typedef struct {
	char* method;
	char* path;
} HTTPHeaders;


void free_http_headers(HTTPHeaders* head){
	if(head->method){
		free(head->method);
	}
	if(head->path){
		free(head->path);
	}
}


char* html_header_get_data(const char* start){
	int i = 0;
	while(start[i] != ' ' && start[i] != '\r' && start[i] != '\n'){
		i++;
	}
	char* out = malloc(sizeof(char)*i+1);
	memcpy(out, start, sizeof(char)*i);
	out[i] = '\0';
	return out;
}


HTTPHeaders html_parse_header(String_Builder* header){
	HTTPHeaders http_head = {0};
	char* method = html_header_get_data(header->items);
	char* path = html_header_get_data(header->items + strlen(method) + 1);

	http_head.method = method;
	http_head.path = path;

	return http_head; 
}


const char* html_get_mime_type(HTTPHeaders* head){
	int start = 0;
	for(int i = strlen(head->path); i > 0; i--){
		if(head->path[i] == '.'){
			start = i;
			break;
		}
	}
	if(strncmp(head->path+start, ".html", 5) == 0){
		return "text/html";
	}else if(strncmp(head->path+start, ".css", 4) == 0){
		return "text/css";
	}else{
		return "text/plain";
	}
}


bool html_get_requested_file(String_Builder* sb, HTTPHeaders* headers){
	char* path = nob_temp_sprintf("%s%s", ALLOWED_DIR, headers->path);
	bool error = false;

	if(!is_inside_folder(path, ALLOWED_DIR)){
		error = true;
		goto defer;
	}

	Nob_File_Type file_type = nob_get_file_type(path);
	if(file_type < 0){
		error = true;
		goto defer;
	} else if(file_type == NOB_FILE_DIRECTORY){
		char* index_path = nob_temp_sprintf("%sindex.html", path);
		Nob_File_Type index_ft = nob_get_file_type(index_path);
		if(index_ft < 0){
			error = true;
			goto defer;
		}else if(index_ft == NOB_FILE_REGULAR){
			if(!nob_read_entire_file(index_path, sb)){
				error = true;
				goto defer;
			}else{
				free(headers->path);
				headers->path = strdup(index_path);
			}
		}
	}else if(file_type == NOB_FILE_REGULAR){
		if(!nob_read_entire_file(path, sb)){
			error = true;
			goto defer;
		}
	}

defer:
	nob_temp_reset();
	return error;
}


void html_get_request(tcpsock s, String_Builder* out){
	char buf[4096];
	int i = 0;
	int nl = 0;
	int r = 0;
	while(1){
		size_t n = tcprecvuntil(s, buf, sizeof(buf), "\r\n\r\n", 4, -1);
		if(n == 0) break;
		if(n == 1){
			if(buf[0] == '\n'){
				nl++;
			}else if(buf[0] == '\r'){
				r++;
			}
			i++;
		}else{
			i = 0;
			nl = 0;
			r = 0;
		}
		// End of headers signature
		if(i == 3 && nl == 2 && r == 1){
			break;
		}
		if(n != 1){
			sb_append_buf(out, buf, n);			
		}
	}
}


void html_send_file_error(tcpsock s){
	char response[1024];
	int len = snprintf(
	    response,
	    sizeof(response),
	    "HTTP/1.0 401 OK\r\n"
	    "Content-Type: application/json; charset=UTF-8\r\n"
	    "Content-Length: 62\r\n"
	    "Connection: close\r\n"
	    "\r\n{\"error\": \"This file does not exist or your cannot access it\"}"
	);
	tcpsend(s, response, len, -1);
}


void html_send_file(tcpsock s, HTTPHeaders* http_header,String_Builder* file){
	char response[1024*16];
	
	int len = snprintf(
	    response,
	    sizeof(response),
	    "HTTP/1.0 200 OK\r\n"
	    "Content-Type: %s; charset=UTF-8\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    html_get_mime_type(http_header),
	    (size_t)file->count
	);
	tcpsend(s, response, len, -1);
	tcpsend(s, file->items, file->count, -1);
}


coroutine void html_client(tcpsock s){
	int status_code = 200;
	String_Builder full_message = {0};

	html_get_request(s, &full_message);

	HTTPHeaders http_header = html_parse_header(&full_message);

	String_Builder file = {0};
	bool error = html_get_requested_file(&file, &http_header);
	if(error){
		status_code = 401;
		html_send_file_error(s);
		goto defer;
	}

	html_send_file(s, &http_header, &file);
	
defer:	
	tcpflush(s, -1);
	tcpclose(s);

	printf("[%s] %s - %d\n", http_header.method, http_header.path, status_code);
	
	if(full_message.items){
		free(full_message.items);
	}
	free_http_headers(&http_header);
	if(file.items){
		free(file.items);
	}
}
#endif