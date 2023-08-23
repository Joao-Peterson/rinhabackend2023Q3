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

	db_close(db);

	// char *port = getenv("SERVER_PORT");

	// http_listen(port, NULL, .on_request = on_request, .log = 1);

	// fio_start(.threads = 4);

	return 0;
}