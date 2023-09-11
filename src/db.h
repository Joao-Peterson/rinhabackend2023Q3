#ifndef _DB_HEADER_
#define _DB_HEADER_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define DB_ERROR_MSG_LEN 300

// ------------------------------------------------------------ Types --------------------------------------------------------------

// db type
typedef enum{
	db_vendor_postgres,
	db_vendor_postgres15,
	// db_vendor_mysql,
	// db_vendor_firebird,
	// db_vendor_cassandra,
	db_vendor_invalid
}db_vendor_t;

// the type for the query parameter
typedef enum{
	db_type_invalid = -1,
	db_type_integer = 0,
	db_type_bool,
	db_type_float,
	db_type_string,
	db_type_blob,
	db_type_null
}db_type_t;

// db param used in the query
typedef struct{
	db_type_t type;
	bool is_array;
	size_t count;
	size_t size;
	void *value;
}db_param_t;

// entry on database. Returned by a query. Represents a single row
typedef struct{
	char **names;
	char **values;
}db_entry_t;

// error codes
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

// current state of the db object
typedef enum{
	db_state_uninitialized,
	db_state_not_connected,
	db_state_connected,
	db_state_invalid_db
}db_state_t;

// db struct
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

// ------------------------------------------------------------ Functions ----------------------------------------------------------

// create a new db object
db_t *db_create(db_vendor_t type, char *host, char *port, char *database, char *user, char *password, char *role);

// connect to database 
db_error_code_t db_connect(db_t *db);

// close connection
void db_close(db_t *db);

// new integer param for query
db_param_t db_param_integer(int *value);

// new bool param for query
db_param_t db_param_bool(bool *value);

// new float param for query
db_param_t db_param_float(float *value);

// new string param for query
db_param_t db_param_string(char *value);

// new blob param for query
// db_param_t db_param_blob(void *value, size_t size);

// new null param for query
db_param_t db_param_null();

// new integer array param for query
db_param_t db_param_integer_array(int **value, size_t count);

// new bool array param for query	
db_param_t db_param_bool_array(bool **value, size_t count);

// new float array param for query	
db_param_t db_param_float_array(float **value, size_t count);

// new string array param for query	
db_param_t db_param_string_array(char **value, size_t count);

// // new blob array param for query	
// db_param_t db_param_blob_array(void **value, size_t count, size_t size_elem);

// exec a query
db_error_code_t db_exec(db_t *db, char *query, size_t params_count, ...);

// clean results from last query 
void db_clean_results(db_t *db);

// read results from last query
char *db_results_read(db_t *db, uint32_t entry, uint32_t field);

// print results from last query
void db_print_results(db_t *db);

// return last results as json string
char *db_json_entries(db_t *db, bool squash_if_single);

#endif