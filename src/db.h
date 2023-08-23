#ifndef _DB_HEADER_
#define _DB_HEADER_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define DB_ERROR_MSG_LEN 300

typedef enum{
	db_vendor_postgres,
	db_vendor_postgres15,
	// db_vendor_mysql,
	// db_vendor_firebird,
	// db_vendor_cassandra,
	db_vendor_invalid
}db_vendor_t;

typedef enum{
	db_type_integer,
	db_type_bool,
	db_type_float,
	db_type_string,
	db_type_blob,
	db_type_null
}db_type_t;

typedef struct{
	db_type_t type;
	size_t size;
	char *value;
}db_param_t;

typedef struct{
	char **names;
	char **values;
}db_entry_t;

typedef enum{
	db_error_code_ok = 0,
	db_error_code_zero_results,
	db_error_code_unique_constrain_violation,
	db_error_code_invalid_type,
	db_error_code_invalid_range,
	db_error_code_processing,
	db_error_code_info,
	db_error_code_fatal,
	db_error_code_connection_error,
	db_error_code_unknown,
	db_error_code_invalid_db,
	db_error_code_max
}db_error_code_t;

typedef enum{
	db_state_uninitialized,
	db_state_not_connected,
	db_state_connected,
	db_state_invalid_db
}db_state_t;

typedef struct{
	db_vendor_t vendor;
	char *host;
	char *port;
	char *database;
	char *user;
	char *password;
	char *role;

	int64_t results_count;
	int64_t results_field_count;
	db_entry_t *results;

	db_state_t state;
	db_error_code_t error_code;
	char error_msg[DB_ERROR_MSG_LEN];

	void *context;
}db_t;

db_t *db_create(db_vendor_t type, char *host, char *port, char *database, char *user, char *password, char *role);

db_error_code_t db_connect(db_t *db);

void db_close(db_t *db);

db_param_t db_param_new(db_type_t type, char *value, size_t size);

db_error_code_t db_exec(db_t *db, char *query, size_t params_count, ...);

void db_clean_results(db_t *db);

char *db_results_read(db_t *db, uint32_t entry, uint32_t field);

void db_print_results(db_t *db);

char *db_json_entries(db_t *db, bool squash_if_single);

#endif