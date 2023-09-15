#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "src/varenv.h"
#include "src/db.h"
#include "models/pessoas.h"

int main(int argq, char **argv, char **envp){

	// try load env var from env file
	loadEnvVars(NULL);

	// db connection
	db_t *db = db_create(db_vendor_postgres, 5,
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

	char *stack[] = {
		"c#",
		"c++",
		"c"
	};

	db_results_t *res;

	res = pessoas_insert(db, "nico", "nico", "20000201", 3, stack);
	if(res->code){
		printf("Insert failed. Postgress: %s\n", res->msg);
	}else{
		printf("Insert ok!\n");
	}
	db_results_destroy(res);

	res = pessoas_insert(db, "peterson", "jjpsss peterson joa", "19990206", 3, stack);
	if(res->code){
		printf("Insert failed. Postgress: %s\n", res->msg);
	}else{
		printf("Insert ok!\n");
	}
	db_results_destroy(res);

	res = pessoas_select_search(db, "c#", 50);
	if(res->code){
		printf("Search failed. Postgress: %s\n", res->msg);
	}else{
		printf("Search ok!\n");
		db_print_results(res);
	}
	db_results_destroy(res);

	res = pessoas_select_uuid(db, "4dcc0115-f0e7-486f-92a7-2c18109f1956");
	if(res->code){
		printf("Select uuid failed. Postgress: %s\n", res->msg);
	}else{
		printf("Select uuid ok!\n");
		db_print_results(res);
	}
	db_results_destroy(res);

	res = pessoas_count(db);
	if(res->code){
		printf("Count failed. Postgress: %s\n", res->msg);
	}else{
		printf("Count ok!\n");
		db_print_results(res);
	}
	db_results_destroy(res);

	db_destroy(db);

	return 0;
}