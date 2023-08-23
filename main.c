#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "facil.io/http.h"
#include "inc/varenv.h"
#include "src/utils.h"
#include "src/db.h"

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
	db = db_connect(db_vendor_postgres, getenv("DB_STRING"));
	if(db == NULL){
		printf("Could not connect to database\n");
		exit(1);
	}else{
		printf("Connected to database!\n");
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
	db_count(db);

	if(db->error_code){
		printf("On GET count failed. DB query failed. Postgress: %s\n", db->error_msg);
		http_send_error(h, http_status_code_InternalServerError);
	}
	else{
		char *count = db->last_results->entries[0]->value[0];
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
	db_select_search(db, tquery, 50);
	if(db->error_code){
		printf("On GET search failed. DB query failed. Postgress: %s\n", db->error_msg);
		http_send_error(h, http_status_code_InternalServerError);
	}
	else{
		FIOBJ json = db_json_entries(db, false);
		fio_str_info_s info = fiobj_obj2cstr(json);
		http_send_body(h, info.data, info.len);
		fiobj_free(json);
	}
}

// get uuid
void on_get_uuid(http_s *h){
	if(fiobj_str_substr(h->path, "/pessoas")){									// has sub path pessoas
		char *pathstr = fiobj_obj2cstr(h->path).data;
		char *cursor = pathstr + strlen(pathstr);								// get uuid
		while(cursor > pathstr){
			if(*cursor == '/')
				break;

			cursor--;
		}

		if(cursor == pathstr){													// no uuid
			h->status = http_status_code_NotFound;
			http_send_body(h, "Not found", 9);
		}
		else{																	// if valid uuid
			char *uuid = cursor + 1;

			// db call
			db_select_uuid(db, uuid);
			if(db->error_code){
				printf("On GET uuid failed. DB query failed. Postgress: %s\n", db->error_msg);
				http_send_error(h, http_status_code_InternalServerError);
			}
			else{
				FIOBJ json = db_json_entries(db, true);
				fio_str_info_s info = fiobj_obj2cstr(json);
				http_send_body(h, info.data, info.len);
				fiobj_free(json);
			}
		}
	}
	else{																		// not /pessoas path
		h->status = http_status_code_NotFound;
		http_send_body(h, "Not found", 9);
	}
}

// post
void on_post(http_s *h){
	if(http_parse_body(h)){
		h->status = http_status_code_BadRequest;								// search without query
		http_send_body(h, "Bad request", 11);
	}
	else{
		// get values
		FIOBJ key = fiobj_str_new("", 1);

		fiobj_str_clear(key);
		fiobj_str_write(key, "apelido", 7);
		char *apelido   	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;
		printf("apelido: %s\n", apelido);

		fiobj_str_clear(key);
		fiobj_str_write(key, "nome", 4);
		char *nome      	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;
		printf("nome: %s\n", nome);

		fiobj_str_clear(key);
		fiobj_str_write(key, "nascimento", 10);
		char *nascimento	= fiobj_obj2cstr(fiobj_hash_get(h->params, key)).data;
		printf("nascimento: %s\n", nascimento);

		fiobj_str_clear(key);
		fiobj_str_write(key, "stack", 5);

		FIOBJ stack = fiobj_str_new("{", 1);
		FIOBJ stackobj = fiobj_hash_get(h->params, key);
		size_t stacksize = fiobj_ary_count(stackobj);
		
		// stack value
		for(size_t i = 0; i < stacksize; i++){
			fiobj_str_concat(stack, fiobj_ary_index(stackobj, i));

			if(i != (stacksize - 1))
				fiobj_str_write(stack, ",", 1);
		}
		fiobj_str_write(stack, "}", 1);

		char *stackstr = fiobj_obj2cstr(stack).data;
		printf("stack: %s\n", fiobj_obj2cstr(stack).data);

		// db call
		db_insert(db, nome, apelido, nascimento, stackstr);
		if(db->error_code){
			printf("On POST failed. DB Insert failed. Postgress: %s\n", db->error_msg);
			http_send_error(h, http_status_code_UnprocessableEntity);
		}
		else{
			FIOBJ name = fiobj_str_new("Location", 8);
			FIOBJ value = fiobj_str_new("", 1);
			fiobj_str_printf(value, "/pessoas/%s", db->last_results->entries[0]->value[0]);
			http_set_header(h, name, value);

			fiobj_free(name);
			h->status = http_status_code_Created;
			http_finish(h);
		}

		// free stuff
		fiobj_free(stack);
		fiobj_free(key);
	}
}
