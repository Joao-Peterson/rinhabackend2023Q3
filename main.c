#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "facil.io/http.h"
#include "src/varenv.h"
#include "src/utils.h"
#include "src/db.h"
#include "models/pessoas.h"

// handlers
void on_request(http_s *h);
void on_get(http_s *h);

// get
void on_get_count(http_s *h);
void on_get_uuid(http_s *h);
void on_get_search(http_s *h);

// post
void on_post(http_s *h);

// global db
db_t *db;

// main
int main(int argq, char **argv, char **envp){

	// try load env var from env file
	loadEnvVars(NULL);

	// db connection
	db = db_create(db_vendor_postgres, 
		getenv("DB_HOST"),
		getenv("DB_PORT"),
		getenv("DB_DATABASE"),
		getenv("DB_USER"),
		getenv("DB_PASSWORD"),
		getenv("DB_ROLE")
	);

	if(db == NULL){
		printf("Could not create database object. Host, database or user were passed as NULL\n");
		exit(2);
	}

	if(db_connect(db)){
		printf(db->error_msg);
		exit(1);
	}else{
		printf(db->error_msg);
	}

	char *port = getenv("SERVER_PORT");
	char *threads = getenv("SERVER_THREADS");
	int threads_num = atoi(threads);

	http_listen(port, NULL, .on_request = on_request, .log = 1);

	printf("Starting server on port: [%s]\n", port);
	fio_start(.threads = threads_num);

	printf("Stopping server...\n");

	db_close(db);

	return 0;
}

// main callback
void on_request(http_s *h){
	if(fiobj_str_cmp(h->method, "GET")){
		on_get(h);
	}
	else if(fiobj_str_cmp(h->method, "POST")){
		on_post(h);
	}
	else{
		h->status = http_status_code_MethodNotAllowed;
		http_send_body(h, "Method not allowed", 18);
	}
}

// get
void on_get(http_s *h){
	// printf("value: %s\n", fiobj_obj2cstr(h->query).data);
	
	if(fiobj_str_cmp(h->path, "/contagem-pessoas")){								// count
		on_get_count(h);
	}
	else if(fiobj_str_substr(h->path, "/pessoas")){									// get 

		if(h->query != FIOBJ_INVALID){												// search 
			on_get_search(h);
		}
		else{																		// get by uuid or invalid request
			on_get_uuid(h);
		}
	}
	else{
		h->status = http_status_code_BadRequest;									// search without query
		http_send_body(h, "Bad request", 11);
	}
}

// count
void on_get_count(http_s *h){
	if(pessoas_count(db)){
		printf("On GET count failed. DB query failed. Database: %s\n", db->error_msg);
		http_send_error(h, http_status_code_InternalServerError);
	}
	else{
		char *count = db_results_read(db, 0, 0);
		http_send_body(h, count, strlen(count));
	}
}

// search
void on_get_search(http_s *h){
	http_parse_query(h);

	FIOBJ key = fiobj_str_new("t", 1);
	FIOBJ value = fiobj_hash_get(h->params, key);
	fiobj_free(key);

	if(value == FIOBJ_INVALID){
		h->status = http_status_code_BadRequest;								// search without "t" value
		http_send_body(h, "Bad request", 11);
	}

	char *tquery = fiobj_obj2cstr(value).data;	

	// db call
	switch(pessoas_select_search(db, tquery, 50)){
		case db_error_code_zero_results:
			http_send_body(h, "[]", 2);
			break;			

		case db_error_code_ok:
		{
			char *json = db_json_entries(db, false);
			http_send_body(h, json, strlen(json));
			free(json);
		}
		break;			

		default:
			printf("On GET search failed. DB query failed. Database: %s\n", db->error_msg);
			http_send_error(h, http_status_code_InternalServerError);
	}
}

// get uuid
void on_get_uuid(http_s *h){
	if(!fiobj_str_substr(h->path, "/pessoas")){										// not /pessoas/* path
		h->status = http_status_code_NotFound;
		http_send_body(h, "Not found", 9);
		return;
	}

	char *pathstr = fiobj_obj2cstr(h->path).data;
	char *cursor = pathstr + strlen(pathstr);										// get uuid
	while(cursor > pathstr){
		if(*cursor == '/')
			break;

		cursor--;
	}

	if(cursor == pathstr){															// no uuid
		h->status = http_status_code_NotFound;
		http_send_body(h, "Not found", 9);
		return;
	}

	char *uuid = cursor + 1;

	// db call
	switch(pessoas_select_uuid(db, uuid)){
		case db_error_code_zero_results:
			http_send_error(h, http_status_code_NotFound);
			break;

		case db_error_code_invalid_type:
			h->status = http_status_code_UnprocessableEntity;
			http_send_body(h, db->error_msg, strlen(db->error_msg));
			break;

		case db_error_code_ok:
		{
			char *json = db_json_entries(db, true);
			http_send_body(h, json, strlen(json));
			free(json);
		}
		break;

		default:
			http_send_error(h, http_status_code_InternalServerError);
			break;
	}
}

// post
void on_post(http_s *h){
	if(http_parse_body(h)){
		h->status = http_status_code_BadRequest;								// search without query
		http_send_body(h, "Bad request", 11);
		return;
	}

	// get values
	FIOBJ key = fiobj_str_new("", 1);

	fiobj_str_clear(key);
	fiobj_str_write(key, "apelido", 7);
	char *apelido   	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	fiobj_str_clear(key);
	fiobj_str_write(key, "nome", 4);
	char *nome      	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	fiobj_str_clear(key);
	fiobj_str_write(key, "nascimento", 10);
	char *nascimento	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	fiobj_str_clear(key);
	fiobj_str_write(key, "stack", 5);

	FIOBJ stackobj = fiobj_hash_get(h->params, key);
	size_t stacksize = fiobj_ary_count(stackobj);
	char *stack[stacksize];
	
	// stack value
	for(size_t i = 0; i < stacksize; i++){
		stack[i] = fiobj_obj2cstr(fiobj_ary_index(stackobj, i)).data;
	}

	// db call
	switch(pessoas_insert(db, nome, apelido, nascimento, stacksize, stack)){
		case db_error_code_ok:
		{
			// header Location
			FIOBJ name = fiobj_str_new("Location", 8);
			FIOBJ value = fiobj_str_new(NULL, 0);
			fiobj_str_printf(value, "/pessoas/%s", db_results_read(db, 0, 0));
			http_set_header(h, name, value);

			// json body id
			FIOBJ json = fiobj_str_new(NULL, 0);
			fiobj_str_printf(json, "{\"id\":\"%s\"}", db_results_read(db, 0, 0));
			fio_str_info_s jsonstr = fiobj_obj2cstr(json);
			
			h->status = http_status_code_Created;
			http_send_body(h, jsonstr.data, jsonstr.len);

			fiobj_free(name);
			fiobj_free(json);
		}
		break;

		case db_error_code_invalid_range:
		case db_error_code_invalid_type:
		case db_error_code_unique_constrain_violation:
			h->status = http_status_code_UnprocessableEntity;
			http_send_body(h, db->error_msg, strlen(db->error_msg));
			break;

		default:
			http_send_error(h, http_status_code_InternalServerError);
			break;
	}

	// free stuff
	fiobj_free(key);
}

