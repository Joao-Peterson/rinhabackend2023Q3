#include "db.h"
#include "db_priv.h"
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>
#include "string+.h"
#include "db_postgres.h"

// ------------------------------------------------------------ Invalid database default

// default connection function
static db_error_code_t db_default_function_invalid(db_t *db){
	db->state = db_state_invalid_db;
	return db_error_code_invalid_db;
}

// ------------------------------------------------------------- Private calls  ----------------------------------------------------

// create new result object
db_results_t *db_results_new(int64_t entries, int64_t fields, db_error_code_t code, char *msg){
	db_results_t *result = calloc(1, sizeof(db_results_t));

	result->entries_count = entries;
	result->fields_count = fields;
	result->code = code;

	if(msg != NULL) memcpy(result->msg, msg, strlen(msg) + 1);

	return result;
}

// create new result fmt
db_results_t *db_results_new_fmt(int64_t entries, int64_t fields, db_error_code_t code, char *fmt, ...){
	va_list args;
	va_start(args, fmt);

	db_results_t *result = calloc(1, sizeof(db_results_t));

	result->entries_count = entries;
	result->fields_count = fields;
	result->code = code;

	vsnprintf(result->msg, DB_MSG_LEN - 1 ,fmt, args);

	va_end(args);
	return result;
}

// create new result object for null db
db_results_t *db_result_new_nulldb(){
	return db_results_new(0, 0, db_error_code_invalid_db, "Database passed was null");
}

// set reasult error
void db_results_set_message(db_results_t *results, char *msg, db_vendor_t vendor, char *vendor_msg){
	snprintf(results->msg, DB_MSG_LEN, "%s. (%s): %s\n", msg, db_vendor_name_map(vendor), vendor_msg);
}

// ------------------------------------------------------------ Database maps ------------------------------------------------------

// vendor name map
static const char *db_vendor_name_map(db_vendor_t vendor){
	switch(vendor){
		default: return "Invalid DB vendor";
		case db_vendor_postgres: 
		case db_vendor_postgres15: 
			return "Postgres 15";
	}
}

// map error codes
db_error_code_t db_error_code_map(db_vendor_t vendor, int code){
	switch(vendor){
		default: 						
			return db_error_code_invalid_db;

		case db_vendor_postgres: 		
		case db_vendor_postgres15: 		
			return db_error_code_map_postgres(code);
	}
}

// connection functions
db_error_code_t db_connect_function_map(db_t *db){
	if(db == NULL) return db_error_code_invalid_db;
	
	switch(db->state){
		// only try connecting/reconnecting
		case db_state_not_connected:
		case db_state_failed_connection:
			break;

		case db_state_connected:
			return db_error_code_ok;

		case db_state_connecting:
			return db_error_code_processing;
		
		default:
		case db_state_invalid_db:
			return db_error_code_invalid_db;
	}

	switch(db->vendor){
		default: 
			return db_default_function_invalid(db);
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return db_connect_function_postgres(db);
	}

	return db_error_code_unknown;
}

// poll current db status. map
db_state_t db_stat_function_map(db_t *db){
	if(db == NULL) return db_state_invalid_db;
	
	switch(db->vendor){
		default: 
			return db_state_invalid_db;
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return db_stat_function_postgres(db);
	}
}

// close db map
void db_destroy_function_map(db_t *db){
	if(db == NULL) return;
	
	free(db->host);
	free(db->port);
	free(db->database);
	free(db->user);
	free(db->password);
	free(db->role);
	
	// context free
	switch(db->vendor){
		default: return;
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			db_destroy_function_postgres(db);
	}

	free(db);
}

// exec query map
static db_results_t *db_exec_function_map(db_t *db, void *connection, char *query, size_t params_count, va_list params){
	if(db == NULL) return db_result_new_nulldb();

	switch(db->vendor){
		default: 
			return db_results_new(0, 0, db_error_code_invalid_db, "Vendor not yet implemented");
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return db_exec_function_postgres(db, connection, query, params_count, params);
	}
}

// port map 
static char *db_default_port_map(db_vendor_t vendor){
	switch(vendor){
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return "5432";

		default:
			return "3306";
	}
}

// ------------------------------------------------------------- Public calls ------------------------------------------------------

// create db object
db_t *db_create(db_vendor_t type, size_t num_connections, char *host, char *port, char *database, char *user, char *password, char *role, db_error_code_t *code){
	if(
		(host == NULL) ||
		(database == NULL) ||
		(user == NULL) ||
		(num_connections < 1)
	){
		if(code != NULL) *code = db_error_code_invalid_db;
		return NULL;
	}

	db_t *db = (db_t*)calloc(1, sizeof(db_t));

	db->context.connections_count = num_connections;
	if(pthread_mutex_init(&(db->context.connections_lock), NULL) != 0){
		if(code != NULL) *code = db_error_code_unknown;
		free(db);
		return NULL;
	}

	db->host     	= strdup(host);
	db->port     	= port != NULL ? strdup(port) : strdup(db_default_port_map(type));
	db->database 	= strdup(database);
	db->user     	= strdup(user);
	db->password 	= password != NULL ? strdup(password) : strdup("");
	db->role     	= role != NULL ? strdup(role) : strdup("");
	db->state 		= db_state_not_connected;
	
	if(code != NULL) *code = db_error_code_ok;
	return db;
}

// connect to database
db_error_code_t db_connect(db_t *db){
	return db_connect_function_map(db);
}

// poll current db status. Use this function before accessing db->state
db_state_t db_stat(db_t *db){
	return db_stat_function_map(db);
}

// new param for query
db_param_t db_param_new(db_type_t type, bool is_array, size_t count, void *value, size_t size){
	bool is_invalid = false;
	
	// chech for null params on arrays
	if(is_array && count > 0){
		switch(type){
			case db_type_integer:
			{
				int **array = (int **)value;
				for(size_t i = 0; i < count; i++){
					if(array[i] == NULL)
						is_invalid = true;
				}
			}
			break;
				
			case db_type_bool:
			{
				bool **array = (bool **)value;
				for(size_t i = 0; i < count; i++){
					if(array[i] == NULL)
						is_invalid = true;
				}
			}
			break;
				
			case db_type_float:
			{
				float **array = (float **)value;
				for(size_t i = 0; i < count; i++){
					if(array[i] == NULL)
						is_invalid = true;
				}
			}
			break;
				
			case db_type_string:
			{
				char **array = (char **)value;
				for(size_t i = 0; i < count; i++){
					if(array[i] == NULL)
						is_invalid = true;
				}
			}
			break;
				
			case db_type_blob:
			{
				void **array = (void **)value;
				for(size_t i = 0; i < count; i++){
					if(array[i] == NULL)
						is_invalid = true;
				}
			}
			break;
				
			default:
			case db_type_invalid:
			case db_type_null:
				break;
		}
	}

	// checks
	if(
		(!is_array && size == 0) ||
		(is_invalid)
	){
		db_param_t invalid = {
			.type = db_type_invalid,
			.is_array = false,
			.count = 0,
			.value = NULL,
			.size = 0
		};

		return invalid;
	}
	
	// return param
	db_param_t param = {
		.type = type,
		.is_array = is_array,
		.count = count,
		.value = is_array && count == 0 ? NULL : value,
		.size = size
	};

	return param; 
}

// new integer param for query
db_param_t db_param_integer(int *value){
	return db_param_new(db_type_integer, false, 0, (void*)value, sizeof(int)); 
}

// new bool param for query
db_param_t db_param_bool(bool *value){
	return db_param_new(db_type_bool, false, 0, (void*)value, sizeof(bool)); 
}

// new float param for query
db_param_t db_param_float(float *value){
	return db_param_new(db_type_float, false, 0, (void*)value, sizeof(float)); 
}

// new string param for query
db_param_t db_param_string(char *value){
	return db_param_new(db_type_string, false, 0, (void*)value, strlen(value != NULL ? value : "") + 1); 
}

// TODO add blob type param
// // new blob param for query
// db_param_t db_param_blob(void *value, size_t size){
// 	return db_param_new(db_type_blob, false, 0, (void*)value, size); 
// }

// new null param for query
db_param_t db_param_null(){
	return db_param_new(db_type_null, false, 0, NULL, 1); 
}

// new integer array param for query
db_param_t db_param_integer_array(int **value, size_t count){
	return db_param_new(db_type_integer_array, true, count, (void*)value, sizeof(int)); 
}

// new bool array param for query	
db_param_t db_param_bool_array(bool **value, size_t count){
	return db_param_new(db_type_bool_array, true, count, (void*)value, sizeof(bool)); 
}

// new float array param for query	
db_param_t db_param_float_array(float **value, size_t count){
	return db_param_new(db_type_float_array, true, count, (void*)value, sizeof(float)); 
}

// new string array param for query	
db_param_t db_param_string_array(char **value, size_t count){
	return db_param_new(db_type_string_array, true, count, (void*)value, 1); 
}

// TODO add blob array type param
// // new blob array param for query	
// db_param_t db_param_blob_array(void **value, size_t count, size_t size_elem){
// 	return db_param_new(db_type_blob, true, count, (void*)value, size_elem); 
// }

// try and get a connection
static void *db_request_conn(db_t *db){
	switch(db->vendor){
		default:
		case db_vendor_invalid:
			return NULL;
			break;
		
		case db_vendor_postgres15:
		case db_vendor_postgres:
			return db_request_conn_postgres(db);
			break;
	}
}

// return used connection
static void db_return_conn(db_t *db, void *conn){
	switch(db->vendor){
		default:
		case db_vendor_invalid:
			break;
		
		case db_vendor_postgres15:
		case db_vendor_postgres:
			db_return_conn_postgres(db, conn);
			break;
	}
}

// exec query
db_results_t *db_exec(db_t *db, char *query, size_t params_count, ...){
	va_list params;
	va_start(params, params_count);
	
	void *conn;
	int retries = DB_CONN_POOL_RETRY;
	while(retries){
		conn = db_request_conn(db);

		if(conn == NULL)
			retries--;
		else
			break;
	}

	if(retries == 0 && conn == NULL)
		return db_results_new_fmt(0, 0, db_error_code_fatal, "Could not get connnection from connection pool. Connection available: [%lu]. Connection count: [%lu]", db->context.available_connection, db->context.connections_count);

	db_results_t *res = db_exec_function_map(db, conn, query, params_count, params);

	db_return_conn(db, conn);

	va_end(params);
	return res;
}

// destroy results
void db_results_destroy(db_results_t *results){
	if(results == NULL) return;

	if(results->fields != NULL){
		for(size_t i = 0; i < results->entries_count; i++){							// for each entry
			for(size_t j = 0; j < results->fields_count; j++){						// on each columns/field
				for(size_t elem = 0; elem < results->entries[i][j].count; elem++){	// on each element if values is array  
					switch(results->entries[i][j].type){
						default:
							break;

						case db_type_integer_array:
							free( ( (int**)(results->entries[i][j].value) ) [elem]);
							break;
							
						case db_type_bool_array:
							free( ( (bool**)(results->entries[i][j].value) ) [elem]);
							break;

						case db_type_float_array:
							free( ( (float**)(results->entries[i][j].value) ) [elem]);
							break;

						case db_type_string_array:
							free( ( (char**)(results->entries[i][j].value) ) [elem]);
							break;

						// case db_type_blob_array:
					}
				}

				free(results->entries[i][j].value);									// free value
			}

			free(results->entries[i]);												// free whole entry
		}

		for(size_t j = 0; j < results->fields_count; j++)
			free(results->fields[j]);												// free fields names
	}

	free(results->entries);
	free(results->fields);
	free(results);
}

// close db connection
void db_destroy(db_t *db){
	db_destroy_function_map(db);
}

// print
void db_print_results(db_results_t *results){
	if(results->entries == NULL) return;

	for(size_t i = 0; i < results->entries_count; i++){
		// columns names
		if(i == 0){
			for(size_t j = 0; j < results->fields_count; j++){
				printf("| %s ", results->fields[j]);

				if(j == (results->fields_count - 1)){
					printf("|\n");
				}
			}	
		}

		// entry
		for(size_t j = 0; j < results->fields_count; j++){
			printf("| ");

			switch(results->entries[i][j].type){
				default:
				case db_type_invalid:
				case db_type_null:
					printf("null");
					break;

				case db_type_integer:
					printf("%d", *db_results_read_integer(results, i, j));
					break;

				case db_type_bool:
					printf("%d", *db_results_read_bool(results, i, j));
					break;
					

				case db_type_float:
					printf("%f", *db_results_read_float(results, i, j));
					break;

				case db_type_string:
					printf("\"%s\"", db_results_read_string(results, i, j));
					break;

				// case db_type_blob:

				case db_type_integer_array:
				{
					uint32_t count;
					int **array = db_results_read_integer_array(results, i, j, &count);

					printf("[");
					for(uint32_t k = 0; k < count; k++){
						if(k != 0)
							printf(",");

						printf("%d", *(array[k]));
					}

					printf("]");
				}
				break;

				case db_type_string_array:
				{
					uint32_t count;
					char **array = db_results_read_string_array(results, i, j, &count);

					printf("[");
					for(uint32_t k = 0; k < count; k++){
						if(k != 0)
							printf(",");

						printf("\"%s\"", array[k]);
					}

					printf("]");
				}
				break;

				// case db_type_bool_array:
				// case db_type_float_array:

				// case db_type_blob_array:
			}

			printf(" ");

			if(j == (results->fields_count - 1)){
				printf("|\n");
			}
		}	
	}
}

// print json
char *db_json_entries(db_results_t *results, bool squash_if_single){
	if(results->entries == NULL) return NULL;

	bool trail = !(squash_if_single && results->entries_count == 1);

	string *json = string_new();

	if(trail)
		string_cat_raw(json, "[");
	else
		string_cat_raw(json, "{");

	for(size_t i = 0; i < results->entries_count; i++){
		if(trail)
			string_cat_raw(json, "{");
	
		for(size_t j = 0; j < results->fields_count; j++){

			if(j != 0)
				string_cat_raw(json, ",");

			// name
			string_cat_fmt(json, "\"%s\":", 50, results->fields[j]);
			
			// value
			switch(results->entries[i][j].type){
				default:
				case db_type_invalid:
				case db_type_null:
					string_cat_fmt(json, "null", 5);
					break;

				case db_type_integer:
					string_cat_fmt(json, "%d", 15, *db_results_read_integer(results, i, j));
					break;

				case db_type_bool:
					string_cat_fmt(json, "%d", 15, *db_results_read_bool(results, i, j));
					break;
					

				case db_type_float:
					string_cat_fmt(json, "%f", 20, *db_results_read_float(results, i, j));
					break;

				case db_type_string:
					string_cat_fmt(json, "\"%s\"", 255, db_results_read_string(results, i, j));
					break;

				// case db_type_blob:

				case db_type_integer_array:
				{
					uint32_t count;
					int **array = db_results_read_integer_array(results, i, j, &count);

					string_cat_raw(json, "[");
					for(uint32_t k = 0; k < count; k++){
						if(k != 0)
							string_cat_raw(json, ",");

						string_cat_fmt(json, "%d", 15, *(array[k]));
					}

					string_cat_raw(json, "]");
				}
				break;

				case db_type_string_array:
				{
					uint32_t count;
					char **array = db_results_read_string_array(results, i, j, &count);

					string_cat_raw(json, "[");
					for(uint32_t k = 0; k < count; k++){
						if(k != 0)
							string_cat_raw(json, ",");

						string_cat_fmt(json, "\"%s\"", 255, array[k]);
					}

					string_cat_raw(json, "]");
				}
				break;

				// case db_type_bool_array:
				// case db_type_float_array:

				// case db_type_blob_array:
			}
		}	

		if(trail)
			string_cat_raw(json, "}");

		if(i != (results->entries_count - 1))
			string_cat_raw(json, ",");
	}

	if(trail)
		string_cat_raw(json, "]");
	else
		string_cat_raw(json, "}");

	char *ret = strdup(json->raw);
	string_destroy(json);

	return ret;
}

// get single entry
db_entry_t *db_results_get_entry(db_results_t *results, uint32_t entry, uint32_t field){
	if(results->entries == NULL) return NULL;
	if(entry >= results->entries_count) return NULL;
	if(field >= results->fields_count) return NULL;
	if(results->entries[entry][field].type == db_type_invalid) return NULL;

	return &(results->entries[entry][field]);
}

// get single entry ith type checking
db_entry_t *db_results_get_entry_tc(db_results_t *results, uint32_t entry, uint32_t field, db_type_t type_check){
	db_entry_t *value = db_results_get_entry(results, entry, field);
	if(value->type != type_check) return NULL;
	return value;
}

// get value from entry with type check
void *db_results_get_value(db_results_t *results, uint32_t entry, uint32_t field, db_type_t type_check){
	db_entry_t *value = db_results_get_entry_tc(results, entry, field, type_check);
	if(value == NULL) return NULL;
	return value->value;
}

// get value from entry for array type with type check
void *db_results_get_array_value(db_results_t *results, uint32_t entry, uint32_t field, db_type_t type_check, uint32_t *length){
	db_entry_t *value = db_results_get_entry_tc(results, entry, field, type_check);
	if(value == NULL){
		if(length != NULL) *length = 0;
		return NULL;
	}

	if(length != NULL) *length = value->count;
	return value->value;
}

// read integer value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
int *db_results_read_integer(db_results_t *results, uint32_t entry, uint32_t field){
	return (int*)db_results_get_value(results, entry, field, db_type_integer);
}

// read bool value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
bool *db_results_read_bool(db_results_t *results, uint32_t entry, uint32_t field){
	return (bool*)db_results_get_value(results, entry, field, db_type_bool);
}

// read float value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
float *db_results_read_float(db_results_t *results, uint32_t entry, uint32_t field){
	return (float*)db_results_get_value(results, entry, field, db_type_float);
}

// read string value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
char *db_results_read_string(db_results_t *results, uint32_t entry, uint32_t field){
	return (char*)db_results_get_value(results, entry, field, db_type_string);
}

// read blob value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
void *db_results_read_blob(db_results_t *results, uint32_t entry, uint32_t field){
	return (void*)db_results_get_value(results, entry, field, db_type_blob);
}

// read integer_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
int **db_results_read_integer_array(db_results_t *results, uint32_t entry, uint32_t field, uint32_t *length){
	return (int**)db_results_get_array_value(results, entry, field, db_type_integer_array, length);
}

// read string_array value from the results of a query. NULL if null | non existent | invalid. Use db_results_isvalid() | db_results_isnull() | db_results_isvalid_and_notnull() to check if the value is what you expect
char **db_results_read_string_array(db_results_t *results, uint32_t entry, uint32_t field, uint32_t *length){
	return (char**)db_results_get_array_value(results, entry, field, db_type_string_array, length);
}

// db_results_read_bool_array(db_results_t *results, uint32_t entry, uint32_t field){

// }

// db_results_read_float_array(db_results_t *results, uint32_t entry, uint32_t field){

// }

// db_results_read_blob_array(db_results_t *results, uint32_t entry, uint32_t field){

// }

// check if a value from the results of a query is valid. Values can be invalid the type on the database is unknown, like when user defined, after database version changes or even if corruption occurs
bool db_results_isvalid(db_results_t *results, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(results, entry, field);
	if(value == NULL) return false;
	return true;
}

// check if a value from the results of a query is null. Invalid values are not null, only a valid value can be null. Valid values can be null or the value itself 
bool db_results_isnull(db_results_t *results, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(results, entry, field);
	if(value == NULL) return false;
	return results->entries[entry][field].type == db_type_null;
}

// check if a value from the results of a query is valid and not null, thus it can be read and used normally without fear of segfaults by NULL ptr access 
bool db_results_isvalid_and_notnull(db_results_t *results, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(results, entry, field);
	if(value == NULL) return false;
	return (results->entries[entry][field].type != db_type_null);
}
