#include "db.h"
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>

// set error Ok and status 0
void db_error_ok(db_t *db){
	db->error_msg = "Ok";
	db->error_code = 0;
}

// set error to message and status 1
void db_error_readmessage(db_t *db){
	db->error_msg = PQerrorMessage(db->context);
	db->error_code = 1;
}

// set error to message and status from database
void db_error_readmessageStatus(db_t *db){
	db->error_msg = PQerrorMessage(db->context);
	db->error_code = PQstatus(db->context);
}

// connect to a db
db_t *db_connect(db_vendor_t type, char *conString){
	if(conString == NULL)
		return NULL;
	
	PGconn *conn = PQconnectdb(conString);

    if (PQstatus(conn) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed. Postgres: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
    }

	db_t *db = (db_t*)calloc(1, sizeof(db_t));

	db->context = (void*)conn;
	return db;
}

// drop and create new tables
void db_migrate(db_t *db, bool dropTables){
	PGresult *res;
	
	char *query;
	if(dropTables){
		query = "drop table pessoas;"
		"create table pessoas("
			"id uuid primary key,"
			"apelido varchar(32) unique not null,"
			"nome varchar(100),"
			"nascimento varchar(8) not null,"
			"stack varchar(32)[]"
        ");";
	}else{
		query = "create table pessoas("
			"id uuid primary key,"
			"apelido varchar(32) unique not null,"
			"nome varchar(100),"
			"nascimento varchar(8) not null,"
			"stack varchar(32)[]"
        ");";
	}

	res = PQexec(db->context, query);
        
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		db_error_readmessage(db);
    }
	else{
		db_error_ok(db);
	}
    
    PQclear(res);
}

// entries
entries_t *processEntries(PGresult *res){
	size_t size_cols = PQnfields(res);
	size_t size_entries = PQntuples(res);

	entries_t *entries = malloc(sizeof(entries_t));
	entries->amount = size_entries;
	entries->entries = malloc(sizeof(entry_t*) * size_entries);
	for(size_t i = 0; i < size_entries; i++){
		entries->entries[i] = malloc(sizeof(entry_t));

		entries->entries[i]->size = size_cols;
		entries->entries[i]->names = malloc(sizeof(char *) * size_cols);
		entries->entries[i]->value = malloc(sizeof(char *) * size_cols);

		for(size_t j = 0; j < size_cols; j++){
			entries->entries[i]->names[j] = PQfname(res, j);
			entries->entries[i]->value[j] = PQgetvalue(res, i, j);
		}
	}

	return entries;
}

// delete
void deleteEntries(entries_t *entries){
	for(size_t i = 0; i < entries->amount; i++){
		free(entries->entries[i]->names);
		free(entries->entries[i]->value);
		free(entries->entries[i]);
	}

	free(entries->entries);
	free(entries);
}

// print
void db_print_entries(db_t *db){
	if(db->last_results == NULL)
		return;

	for(size_t i = 0; i < db->last_results->amount; i++){
		// columns names
		if(i == 0){
			for(size_t j = 0; j < db->last_results->entries[0]->size; j++){
				printf("| %s ", db->last_results->entries[0]->names[j]);

				if(j == (db->last_results->entries[0]->size - 1)){
					printf("|\n");
				}
			}	
		}

		// entry
		for(size_t j = 0; j < db->last_results->entries[i]->size; j++){
			printf("| %s ", db->last_results->entries[i]->value[j]);

			if(j == (db->last_results->entries[i]->size - 1)){
				printf("|\n");
			}
		}	
	}
}

// print josn
FIOBJ db_json_entries(db_t *db, bool squash_if_single){
	if(db->last_results == NULL)
		return;

	bool trail = !(squash_if_single && db->last_results->amount == 1);

	FIOBJ json;

	if(trail)
		json = fiobj_str_new("[", 1);
	else
		json = fiobj_str_new("{", 1);

	for(size_t i = 0; i < db->last_results->amount; i++){
		if(trail)
			fiobj_str_write(json, "{", 1);
	
		for(size_t j = 0; j < db->last_results->entries[i]->size; j++){
			fiobj_str_printf(json, "\"%s\":\"%s\"", db->last_results->entries[i]->names[j], db->last_results->entries[i]->value[j]);

			if(j != (db->last_results->entries[i]->size - 1))
				fiobj_str_write(json, ",", 1);
		}	

		if(trail)
			fiobj_str_write(json, "}", 1);

		if(i != (db->last_results->amount - 1))
			fiobj_str_write(json, ",", 1);
	}
	if(trail)
		fiobj_str_write(json, "]", 1);
	else
		fiobj_str_write(json, "}", 1);

	return json;
}

// insert model into db
void db_insert(db_t *db, char *nome, char *apelido, char *nascimento, char *stack){
	PGresult *res;

	char *query = "insert into pessoas "
	 	"(id, apelido, nome, nascimento, stack) "
	 	"values("
			"gen_random_uuid(),"
			"$1,"
			"$2,"
			"$3,"
			"$4"
		") "
		"returning id";

	char *params[4] = {apelido, nome, nascimento, stack};

	res = PQexecParams(db->context, query, 4, NULL, params, NULL, NULL, 0);
        
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		db_error_readmessage(db);
    }
	else{
		db_error_ok(db);

		if(db->last_results != NULL)
			deleteEntries(db->last_results);

		db->last_results = processEntries(res);
	}	
    
    PQclear(res);
}

// search
void db_select_search(db_t *db, char *searchParam, unsigned int limit){
	PGresult *res;

	char *query = "select * "
		"from pessoas "
		"where ("
			"apelido ilike '%' || $1 || '%' or "
			"nome ilike '%' || $1 || '%' or "
			"$1 = any(stack)"
		") limit $2;";


	char limitstr[10];
	snprintf(limitstr, 9, "%d", limit);

	char *params[2] = {searchParam, limitstr};

	res = PQexecParams(db->context, query, 2, NULL, params, NULL, NULL, 0);
        
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		db_error_readmessage(db);
		if(db->last_results != NULL)
			deleteEntries(db->last_results);
    }
	else{
		db_error_ok(db);

		if(db->last_results != NULL)
			deleteEntries(db->last_results);

		db->last_results = processEntries(res);
	}
    
    PQclear(res);
}

// search
void db_select_uuid(db_t *db, char *uuid){
	PGresult *res;

	char *query = "select * "
		"from pessoas "
		"where id = $1";

	char *params[1] = {uuid};

	res = PQexecParams(db->context, query, 1, NULL, params, NULL, NULL, 0);
        
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		db_error_readmessage(db);
    }
	else{
		db_error_ok(db);

		if(db->last_results != NULL)
			deleteEntries(db->last_results);

		db->last_results = processEntries(res);
	}
    
    PQclear(res);
}

// count
void db_count(db_t *db){
	PGresult *res;

	char *query = "select count(*) from pessoas;";

	res = PQexec(db->context, query);
        
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		db_error_readmessage(db);
    }
	else{
		db_error_ok(db);

		if(db->last_results != NULL)
			deleteEntries(db->last_results);

		db->last_results = processEntries(res);
	}
    
    PQclear(res);
}

// close db connection
void db_close(db_t *db){
    PQfinish(db->context);
	if(db->last_results != NULL)
		deleteEntries(db->last_results);
	free(db);
}