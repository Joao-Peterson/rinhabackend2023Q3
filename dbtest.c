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
	db_t *db = db_create(db_vendor_postgres, 
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

	char *stack[] = {
		"c#",
		"c++",
		"c"
	};

	pessoas_insert(db, "nico", "nico", "20000201", 3, stack);
	if(db->error_code){
		printf("Insert failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Insert ok!\n");
	}

	pessoas_insert(db, "peterson", "jjpsss peterson joa", "19990206", 3, stack);
	if(db->error_code){
		printf("Insert failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Insert ok!\n");
	}

	pessoas_select_search(db, "c#", 50);
	if(db->error_code){
		printf("Search failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Search ok!\n");
		db_print_results(db);
	}

	pessoas_select_uuid(db, "4dcc0115-f0e7-486f-92a7-2c18109f1956");
	if(db->error_code){
		printf("Select uuid failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Select uuid ok!\n");
		db_print_results(db);
	}

	pessoas_count(db);
	if(db->error_code){
		printf("Count failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Count ok!\n");
		db_print_results(db);
	}

	db_close(db);

	return 0;
}