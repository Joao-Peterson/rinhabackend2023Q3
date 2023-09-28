#ifndef _DB_HEADER_
#define _DB_HEADER_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>

#define DB_MSG_LEN 300
#define DB_CONN_POOL_RETRY 5

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
	db_type_integer_array,
	db_type_bool_array,
	db_type_float_array,
	db_type_string_array,
	db_type_blob_array,
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
	db_type_t type;
	size_t count;
	size_t size;
	void *value;
}db_entry_t;

// error codes
typedef enum{
	db_error_code_ok = 0,
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

// returned after database execution calls
typedef struct{
	int64_t fields_count;
	char **fields;
	
	int64_t entries_count;
	db_entry_t **entries;

	db_error_code_t code;
	char msg[DB_MSG_LEN];
}db_results_t;

// current state of the db object
typedef enum{
	db_state_invalid_db = -1,
	db_state_not_connected = 0,
	db_state_connecting,
	db_state_connected,
	db_state_failed_connection
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

	db_state_t state;

	struct{
		pthread_mutex_t connections_lock;
		size_t connections_count;
		void *connections;
		size_t available_connection;
	}context;
}db_t;

// ------------------------------------------------------------ Functions ----------------------------------------------------------

// create a new db object
db_t *db_create(db_vendor_t type, size_t num_connections, char *host, char *port, char *database, char *user, char *password, char *role, db_error_code_t *code);

// connect to database 
db_error_code_t db_connect(db_t *db);

// poll current db status. Use this function before accessing db->state
db_state_t db_stat(db_t *db);

// close connection
void db_destroy(db_t *db);

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

// exec a query. return is always NOT NULL, no need to check
db_results_t *db_exec(db_t *db, char *query, size_t params_count, ...);

// read integer value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
int *db_results_read_integer(db_results_t *results, uint32_t entry, uint32_t field);

// read bool value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
bool *db_results_read_bool(db_results_t *results, uint32_t entry, uint32_t field);

// read float value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
float *db_results_read_float(db_results_t *results, uint32_t entry, uint32_t field);

// read string value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
char *db_results_read_string(db_results_t *results, uint32_t entry, uint32_t field);

// read blob value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
void *db_results_read_blob(db_results_t *results, uint32_t entry, uint32_t field);

// read integer_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
int **db_results_read_integer_array(db_results_t *results, uint32_t entry, uint32_t field, uint32_t *length);

// read string_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
char **db_results_read_string_array(db_results_t *results, uint32_t entry, uint32_t field, uint32_t *length);

// // read bool_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
// db_results_read_bool_array(db_results_t *results, uint32_t entry, uint32_t field);

// // read float_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
// db_results_read_float_array(db_results_t *results, uint32_t entry, uint32_t field);

// // read blob_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
// db_results_read_blob_array(db_results_t *results, uint32_t entry, uint32_t field);

// check if a value from the results of a query is valid. Values can be invalid the type on the database is unknown, like when user defined, after database version changes or even if corruption occurs
bool db_results_isvalid(db_results_t *results, uint32_t entry, uint32_t field);

// check if a value from the results of a query is null. Invalid values are not null, only a valid value can be null. Valid values can be null or the value itself 
bool db_results_isnull(db_results_t *results, uint32_t entry, uint32_t field);

// check if a value from the results of a query is valid and not null, thus it can be read and used normally without fear of segfaults by NULL ptr access 
bool db_results_isvalid_and_notnull(db_results_t *results, uint32_t entry, uint32_t field);

// free results returned from a query
void db_results_destroy(db_results_t *results);

// print results from a query
void db_print_results(db_results_t *results);

// json stringify results from a query
char *db_json_entries(db_results_t *results, bool squash_if_single);

#endif