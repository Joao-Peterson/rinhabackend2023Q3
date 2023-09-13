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
	db_error_set_message(db, "Invalid database vendor", "invalid");
	db->error_code = db_error_code_invalid_db;
	db->state = db_state_invalid_db;
	return db->error_code;
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
		default: 						return db_error_code_invalid_db;
		case db_vendor_postgres: 		
		case db_vendor_postgres15: 		
			return db_error_code_map_postgres(code);
	}
}

typedef db_error_code_t (*db_connect_function_t)(db_t *);

// connection functions
db_error_code_t db_connect_function_map(db_t *db){
	switch(db->vendor){
		default: 
			return db_default_function_invalid(db);
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return db_connect_function_postgres(db);
	}

	return db_error_code_unknown;
}

// close db map
void db_close_function_map(db_t *db){
	db_results_clear(db);
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
			db_close_function_postgres(db);
	}

	free(db);
}

// exec query map
static db_error_code_t db_exec_function_map(db_t *db, char *query, size_t params_count, va_list params){
	db_results_clear(db);

	switch(db->vendor){
		default: 
			return db_default_function_invalid(db);
			
		case db_vendor_postgres:
		case db_vendor_postgres15:
			return db_exec_function_postgres(db, query, params_count, params);
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

// ------------------------------------------------------------ Error handlng ------------------------------------------------------

void db_error_set_message(db_t *db, char *msg, char *vendor_msg){
	snprintf(db->error_msg, DB_ERROR_MSG_LEN, "%s. (%s): %s\n", msg, db_vendor_name_map(db->vendor), vendor_msg);
}

// ------------------------------------------------------------- Public calls ------------------------------------------------------

// create db object
db_t *db_create(db_vendor_t type, char *host, char *port, char *database, char *user, char *password, char *role){
	if(
		(host == NULL) ||
		(database == NULL) ||
		(user == NULL)
	){
		return NULL;
	}

	db_t *db = (db_t*)calloc(1, sizeof(db_t));
	db->host     	= strdup(host);
	db->port     	= port != NULL ? strdup(port) : strdup(db_default_port_map(type));
	db->database 	= strdup(database);
	db->user     	= strdup(user);
	db->password 	= password != NULL ? strdup(password) : strdup("");
	db->role     	= role != NULL ? strdup(role) : strdup("");
	db->state 		= db_state_not_connected;

	db->results_count = -1;
	db->results_field_count = -1;
	
	return db;
}

// connect to database
db_error_code_t db_connect(db_t *db){
	return db_connect_function_map(db);
}

// new param for query
db_param_t db_param_new(db_type_t type, bool is_array, size_t count, void *value, size_t size){
	bool is_invalid = false;
	
	// chech for null params on arrays
	if(is_array){
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

	// check null and empty
	if(
		(value == NULL) || 
		(size == 0) ||
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
		.value = value,
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
	return db_param_new(db_type_integer, true, count, (void*)value, sizeof(int)); 
}

// new bool array param for query	
db_param_t db_param_bool_array(bool **value, size_t count){
	return db_param_new(db_type_bool, true, count, (void*)value, sizeof(bool)); 
}

// new float array param for query	
db_param_t db_param_float_array(float **value, size_t count){
	return db_param_new(db_type_float, true, count, (void*)value, sizeof(float)); 
}

// new string array param for query	
db_param_t db_param_string_array(char **value, size_t count){
	return db_param_new(db_type_string, true, count, (void*)value, 1); 
}

// TODO add blob array type param
// // new blob array param for query	
// db_param_t db_param_blob_array(void **value, size_t count, size_t size_elem){
// 	return db_param_new(db_type_blob, true, count, (void*)value, size_elem); 
// }


// exec query
db_error_code_t db_exec(db_t *db, char *query, size_t params_count, ...){
	db_state_depends_connection(db);
	
	va_list params;
	va_start(params, params_count);
	db_error_code_t code = db_exec_function_map(db, query, params_count, params);
	va_end(params);
	return code;
}

// clean
void db_results_clear(db_t *db){
	if(db->results_count == -1) return;

	for(size_t i = 0; i < db->results_count; i++){
		for(size_t j = 0; j < db->results_field_count; j++){
			switch(db->results[i][j].type){
				default:
					break;

				case db_type_integer_array:
					for(size_t elem = 0; elem < db->results[i][j].count; elem++)
						free( ( (int**)(db->results[i][j].value) ) [elem]);
					break;
					
				case db_type_bool_array:
					for(size_t elem = 0; elem < db->results[i][j].count; elem++)
						free( ( (bool**)(db->results[i][j].value) ) [elem]);
					break;

				case db_type_float_array:
					for(size_t elem = 0; elem < db->results[i][j].count; elem++)
						free( ( (float**)(db->results[i][j].value) ) [elem]);
					break;

				case db_type_string_array:
					for(size_t elem = 0; elem < db->results[i][j].count; elem++)
						free( ( (char**)(db->results[i][j].value) ) [elem]);
					break;

				// case db_type_blob_array:
			}

			free(db->results[i][j].value);
		}

		free(db->results[i]);
	}

	for(size_t j = 0; j < db->results_field_count; j++)
		free(db->result_fields[j]);
	
	free(db->results);
	free(db->result_fields);
	db->results_count = -1;
	db->results_field_count = -1;
}

// close db connection
void db_close(db_t *db){
	db_close_function_map(db);
}

// print
void db_print_results(db_t *db){
	if(db->results_count < 1) return;

	for(size_t i = 0; i < db->results_count; i++){
		// columns names
		if(i == 0){
			for(size_t j = 0; j < db->results_field_count; j++){
				printf("| %s ", db->result_fields[j]);

				if(j == (db->results_count - 1)){
					printf("|\n");
				}
			}	
		}

		// entry
		for(size_t j = 0; j < db->results_field_count; j++){
			printf("| ");

			switch(db->results[i][j].type){
				default:
				case db_type_invalid:
				case db_type_null:
					printf("null");
					break;

				case db_type_integer:
					printf("%d", *db_results_read_integer(db, i, j));
					break;

				case db_type_bool:
					printf("%d", *db_results_read_bool(db, i, j));
					break;
					

				case db_type_float:
					printf("%f", *db_results_read_float(db, i, j));
					break;

				case db_type_string:
					printf("\"%s\"", db_results_read_string(db, i, j));
					break;

				// case db_type_blob:

				case db_type_integer_array:
				{
					uint32_t count;
					int **array = db_results_read_integer_array(db, i, j, &count);

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
					char **array = db_results_read_string_array(db, i, j, &count);

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

			if(j == (db->results_field_count - 1)){
				printf("|\n");
			}
		}	
	}
}

// print josn
char *db_json_entries(db_t *db, bool squash_if_single){
	if(db->results_count < 0) return NULL;

	bool trail = !(squash_if_single && db->results_count == 1);

	string *json = string_new();

	if(trail)
		string_cat_raw(json, "[");
	else
		string_cat_raw(json, "{");

	for(size_t i = 0; i < db->results_count; i++){
		if(trail)
			string_cat_raw(json, "{");
	
		for(size_t j = 0; j < db->results_field_count; j++){

			if(j != 0)
				string_cat_raw(json, ",");

			// name
			string_cat_fmt(json, "\"%s\":", 50, db->result_fields[j]);
			
			// value
			switch(db->results[i][j].type){
				default:
				case db_type_invalid:
				case db_type_null:
					string_cat_fmt(json, "null", 5);
					break;

				case db_type_integer:
					string_cat_fmt(json, "%d", 15, *db_results_read_integer(db, i, j));
					break;

				case db_type_bool:
					string_cat_fmt(json, "%d", 15, *db_results_read_bool(db, i, j));
					break;
					

				case db_type_float:
					string_cat_fmt(json, "%f", 20, *db_results_read_float(db, i, j));
					break;

				case db_type_string:
					string_cat_fmt(json, "\"%s\"", 255, db_results_read_string(db, i, j));
					break;

				// case db_type_blob:

				case db_type_integer_array:
				{
					uint32_t count;
					int **array = db_results_read_integer_array(db, i, j, &count);

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
					char **array = db_results_read_string_array(db, i, j, &count);

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

		if(i != (db->results_count - 1))
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

db_entry_t *db_results_get_entry(db_t *db, uint32_t entry, uint32_t field){
	if(db->results_count == -1) return NULL;
	if(entry >= db->results_count) return NULL;
	if(field >= db->results_field_count) return NULL;
	if(db->results[entry][field].type == db_type_invalid) return NULL;

	return &(db->results[entry][field]);
}

db_entry_t *db_results_get_entry_tc(db_t *db, uint32_t entry, uint32_t field, db_type_t type_check){
	db_entry_t *value = db_results_get_entry(db, entry, field);
	if(value->type != type_check) return NULL;
	return value;
}

void *db_results_get_value(db_t *db, uint32_t entry, uint32_t field, db_type_t type_check){
	db_entry_t *value = db_results_get_entry_tc(db, entry, field, type_check);
	if(value == NULL) return NULL;
	return value->value;
}

void *db_results_get_array_value(db_t *db, uint32_t entry, uint32_t field, db_type_t type_check, uint32_t *length){
	db_entry_t *value = db_results_get_entry_tc(db, entry, field, type_check);
	if(value == NULL){
		if(length != NULL) *length = 0;
		return NULL;
	}

	if(length != NULL) *length = value->count;
	return value->value;
}

int *db_results_read_integer(db_t *db, uint32_t entry, uint32_t field){
	return (int*)db_results_get_value(db, entry, field, db_type_integer);
}

bool *db_results_read_bool(db_t *db, uint32_t entry, uint32_t field){
	return (bool*)db_results_get_value(db, entry, field, db_type_bool);
}

float *db_results_read_float(db_t *db, uint32_t entry, uint32_t field){
	return (float*)db_results_get_value(db, entry, field, db_type_float);
}

char *db_results_read_string(db_t *db, uint32_t entry, uint32_t field){
	return (char*)db_results_get_value(db, entry, field, db_type_string);
}

void *db_results_read_blob(db_t *db, uint32_t entry, uint32_t field){
	return (void*)db_results_get_value(db, entry, field, db_type_blob);
}

int **db_results_read_integer_array(db_t *db, uint32_t entry, uint32_t field, uint32_t *length){
	return (int**)db_results_get_array_value(db, entry, field, db_type_integer_array, length);
}

char **db_results_read_string_array(db_t *db, uint32_t entry, uint32_t field, uint32_t *length){
	return (char**)db_results_get_array_value(db, entry, field, db_type_string_array, length);
}

// db_results_read_bool_array(db_t *db, uint32_t entry, uint32_t field){

// }

// db_results_read_float_array(db_t *db, uint32_t entry, uint32_t field){

// }

// db_results_read_blob_array(db_t *db, uint32_t entry, uint32_t field){

// }

bool db_results_isvalid(db_t *db, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(db, entry, field);
	if(value == NULL) return false;
	return true;
}

bool db_results_isnull(db_t *db, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(db, entry, field);
	if(value == NULL) return false;
	return db->results[entry][field].type == db_type_null;
}

bool db_results_isvalid_and_notnull(db_t *db, uint32_t entry, uint32_t field){
	db_entry_t *value = db_results_get_entry(db, entry, field);
	if(value == NULL) return false;
	return (db->results[entry][field].type != db_type_null);
}
