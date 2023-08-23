#ifndef _DB_HEADER_
#define _DB_HEADER_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum{
	db_vendor_postgres,
	db_vendor_mysql,
	db_vendor_firebird,
	db_vendor_cassandra
}db_vendor_t;

typedef struct{
	size_t size;
	char **names;
	char **value;
}entry_t;

typedef struct{
	size_t amount;
	entry_t **entries;
}entries_t;

typedef struct{
	void *context;
	int error_code;
	char *error_msg;

	entries_t *last_results;

	db_vendor_t type;
}db_t;

db_t *db_connect(db_vendor_t type, char *conString);

void db_close(db_t *db);

void db_migrate(db_t *db, bool dropTables);

void db_insert(db_t *db, char *nome, char *apelido, char *nascimento, char *stack);

void db_select_search(db_t *db, char *searchParam, unsigned int limit);

void db_print_entries(db_t *db);

void db_select_uuid(db_t *db, char *uuid);

void db_count(db_t *db);

#endif