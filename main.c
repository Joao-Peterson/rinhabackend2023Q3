#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "src/string+.h"
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

	char *port = getenv("SERVER_PORT");
	char *workers_env = getenv("SERVER_WORKERS");
	char *threads_env = getenv("SERVER_THREADS");
	int threads = atoi(threads_env);
	int workers = atoi(workers_env);

	// db connection
	db = db_create(db_vendor_postgres, threads,
		getenv("DB_HOST"),
		getenv("DB_PORT"),
		getenv("DB_DATABASE"),
		getenv("DB_USER"),
		getenv("DB_PASSWORD"),
		getenv("DB_ROLE"),
		NULL
	);

	if(db == NULL){
		printf("Could not create database object. Host, database or user were passed as NULL\n");
		exit(2);
	}


	db_connect(db);

	bool wait = true;
	while(wait){
		switch(db_stat(db)){
			default:
				break;

			case db_state_invalid_db:
			case db_state_failed_connection:
				printf("Failed to connect to db\n");
				exit(1);
				break;
			
			case db_state_connected:
				wait = false;
				break;
		}
	}

	// webserver setup
	http_listen(port, NULL, .on_request = on_request, .log = 1);

	printf("Starting server on port: [%s]\n", port);
	fio_start(.threads = threads, .workers = workers);

	printf("Stopping server...\n");

	db_destroy(db);

	return 0;
}

// main callback
void on_request(http_s *h){
	// TODO url parsing: https://facil.io/0.7.x/fio#url-parsing
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
	db_results_t *res = pessoas_count(db, (size_t)pthread_self()); 
	if(res->code){
		printf("On GET count failed. DB query failed. Database: %s\n", res->msg);
		http_send_error(h, http_status_code_InternalServerError);
	}
	else{
		int *count = db_results_read_integer(res, 0, 0);
		string *response = string_sprint("%d", 15, count != NULL ? *count : -1);
		http_send_body(h, response->raw, response->len);

		string_destroy(response);
	}

	db_results_destroy(res);
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
	db_results_t *res = pessoas_select_search(db, (size_t)pthread_self(), tquery, 50);

	if(res->entries_count == 0){
		http_send_body(h, "[]", 2);
		db_results_destroy(res);
		return;
	}
	
	switch(res->code){
		case db_error_code_ok:
		{
			char *json = db_json_entries(res, false);
			http_send_body(h, json, strlen(json));
			free(json);
		}
		break;			

		default:
			printf("On GET search failed. DB query failed. Database: %s\n", res->msg);
			http_send_error(h, http_status_code_InternalServerError);
	}
	
	db_results_destroy(res);
}

// get uuid
void on_get_uuid(http_s *h){
	if(!fiobj_str_substr(h->path, "/pessoas")){										// not /pessoas/* path
		h->status = http_status_code_BadRequest;
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
		h->status = http_status_code_BadRequest;
		http_send_body(h, "Not found", 9);
		return;
	}

	char *uuid = cursor + 1;

	// db call
	db_results_t *res = pessoas_select_uuid(db, (size_t)pthread_self(), uuid);

	if(res->entries_count == 0){
		http_send_error(h, http_status_code_BadRequest);
		db_results_destroy(res);
		return;
	}
	
	switch(res->code){
		case db_error_code_invalid_type:
			h->status = http_status_code_UnprocessableEntity;
			http_send_body(h, res->msg, strlen(res->msg));
			break;

		case db_error_code_ok:
		{
			char *json = db_json_entries(res, true);
			http_send_body(h, json, strlen(json));
			free(json);
		}
		break;

		default:
			http_send_error(h, http_status_code_InternalServerError);
			break;
	}
	
	db_results_destroy(res);
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

	// apelido
	fiobj_str_clear(key);
	fiobj_str_write(key, "apelido", 7);
	char *apelido   	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	// nome
	fiobj_str_clear(key);
	fiobj_str_write(key, "nome", 4);
	char *nome      	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	// nascimento
	fiobj_str_clear(key);
	fiobj_str_write(key, "nascimento", 10);
	char *nascimento	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;

	// stack
	fiobj_str_clear(key);
	fiobj_str_write(key, "stack", 5);

	FIOBJ stackobj = fiobj_hash_get(h->params, key);
	size_t stacksize;
	db_results_t *res;

	if(
		(stackobj == FIOBJ_INVALID) ||
		(FIOBJ_IS_NULL(stackobj)) ||
		(fiobj_ary_count(stackobj) == 0)
	){																				// if no stack
		stacksize = 0;
		res = pessoas_insert(db, (size_t)pthread_self(), nome, apelido, nascimento, stacksize, NULL);
	}
	else{																			// with valid stack
		stacksize = fiobj_ary_count(stackobj);
		char *stack[stacksize];

		// stack value
		for(size_t i = 0; i < stacksize; i++){
			stack[i] = fiobj_obj2cstr(fiobj_ary_index(stackobj, i)).data;
		}

		res = pessoas_insert(db, (size_t)pthread_self(), nome, apelido, nascimento, stacksize, stack);
	}

	// db call
	switch(res->code){
		case db_error_code_ok:
		{
			// header Location
			FIOBJ name = fiobj_str_new("Location", 8);
			FIOBJ value = fiobj_str_new(NULL, 0);
			fiobj_str_printf(value, "/pessoas/%s", db_results_read_string(res, 0, 0));
			http_set_header(h, name, value);

			// json body id
			FIOBJ json = fiobj_str_new(NULL, 0);
			fiobj_str_printf(json, "{\"id\":\"%s\"}", db_results_read_string(res, 0, 0));
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
			http_send_body(h, res->msg, strlen(res->msg));
			break;

		default:
			h->status = http_status_code_InternalServerError;
			http_send_body(h, res->msg, strlen(res->msg));
			break;
	}

	// free stuff
	fiobj_free(key);
	db_results_destroy(res);
}

