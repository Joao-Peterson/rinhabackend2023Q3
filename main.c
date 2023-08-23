#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "facil.io/http.h"
#include "inc/varenv.h"
#include "src/db.h"

void on_request(http_s *req){
	http_send_body(req, "Hello world", 12);
}

int main(int argq, char **argv, char **envp){

	// try load env var from env file
	loadEnvVars(NULL);

	// db connection
	db_t *db = db_connect(db_vendor_postgres, getenv("DB_STRING"));
	if(db == NULL){
		printf("Could not connect to database\n");
		exit(1);
	}else{
		printf("Connected to database!\n");
	}

	printf("Migrating...\n");
	db_migrate(db, false);
	if(db->error_code){
		printf("Migration failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Migration suceeded!\n");
	}


	db_insert(db, "nico", "nico", "20000201", "{c#, c++, c}");
	if(db->error_code){
		printf("Insert failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Insert ok!\n");
	}

	db_select_search(db, "nic", 50);
	if(db->error_code){
		printf("Search failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Search ok!\n");
		db_print_entries(db);
	}

	db_select_uuid(db, "4dcc0115-f0e7-486f-92a7-2c18109f1956");
	if(db->error_code){
		printf("Select uuid failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Select uuid ok!\n");
		db_print_entries(db);
	}

	db_count(db);
	if(db->error_code){
		printf("Count failed. Postgress: %s\n", db->error_msg);
	}else{
		printf("Count ok!\n");
		db_print_entries(db);
	}

	db_close(db);

	// get env
	// char *port = getenv("SERVER_PORT");

	// http_listen(port, NULL, .on_request = on_request, .log = 1);

	// fio_start(.threads = 4);

	return 0;
}