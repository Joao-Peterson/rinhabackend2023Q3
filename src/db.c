#include "db.h"
#include "db_priv.h"
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>
#include "string+.h"
#include "../facil.io/fiobj.h"
#include "../facil.io/fiobj_json.h"

// ------------------------------------------------------------ Database specific --------------------------------------------------

// ------------------------------------------------------------ Postgres

// error map
static db_error_code_t db_error_code_map_postgres(int code){
	switch(code){
		default: 						return db_error_code_unknown;
		case PGRES_EMPTY_QUERY:      	return db_error_code_ok;	        // PGRES_EMPTY_QUERY = 0, 	/* empty query string was executed */
		case PGRES_COMMAND_OK:       	return db_error_code_ok;	        // PGRES_COMMAND_OK,      	/* a query command that doesn't return anything was executed properly by the backend */
		case PGRES_TUPLES_OK:        	return db_error_code_ok;	        // PGRES_TUPLES_OK,       	/* a query command that returns tuples was executed properly by the backend, PGresult contains the result tuples */
		case PGRES_COPY_OUT:         	return db_error_code_processing;	// PGRES_COPY_OUT,        	/* Copy Out data transfer in progress */
		case PGRES_COPY_IN:          	return db_error_code_processing;	// PGRES_COPY_IN,         	/* Copy In data transfer in progress */
		case PGRES_BAD_RESPONSE:     	return db_error_code_fatal;	     	// PGRES_BAD_RESPONSE,   	/* an unexpected response was recv'd from the backend */
		case PGRES_NONFATAL_ERROR:   	return db_error_code_info;	      	// PGRES_NONFATAL_ERROR, 	/* notice or warning message */
		case PGRES_FATAL_ERROR:      	return db_error_code_fatal;			// PGRES_FATAL_ERROR,    	/* query failed */
		case PGRES_COPY_BOTH:        	return db_error_code_processing;	// PGRES_COPY_BOTH,       	/* Copy In/Out data transfer in progress */
		case PGRES_SINGLE_TUPLE:     	return db_error_code_ok;			// PGRES_SINGLE_TUPLE,    	/* single tuple from larger resultset */
		case PGRES_PIPELINE_SYNC:    	return db_error_code_processing;	// PGRES_PIPELINE_SYNC,   	/* pipeline synchronization point */
		case PGRES_PIPELINE_ABORTED: 	return db_error_code_info;	      	// PGRES_PIPELINE_ABORTED	/* Command didn't run because of an abort */
	}
}

static const char *db_connection_string_postgres = "host=%s port=%s user=%s password=%s dbname=%s";

// connection function
static db_error_code_t db_connect_function_postgres(db_t *db){
	char constring[200] = {0};
	snprintf(constring, 199, db_connection_string_postgres, 
		db->host,
		db->port,
		db->user,
		db->password,
		db->database
	);

	PGconn *conn = PQconnectdb(constring);
	db->context = (void*)conn;

    if (PQstatus(conn) == CONNECTION_BAD) {
		db_error_set_message(db, "Connection to database failed", PQerrorMessage(conn));
		db->error_code = db_error_code_connection_error;
		db->state = db_state_not_connected;
		PQfinish(conn);
		return db->error_code;
    }

	db_error_set_message(db, "Connection to dabatase suceeded!", PQerrorMessage(conn));
	db->error_code = db_error_code_ok;
	db->state = db_state_connected;
	return db->error_code;
}

// close db connection
static void db_close_function_postgres(db_t *db){
	PQfinish(db->context);
}

// process entries
static void db_process_entries_postgres(db_t *db, PGresult *res){
	db->results_count = PQntuples(res);
	db->results_field_count = PQnfields(res);

	db->results = malloc(sizeof(db_entry_t) * db->results_count);
	for(size_t i = 0; i < db->results_count; i++){
		db->results[i].names = malloc(sizeof(char *) * db->results_field_count);
		db->results[i].values = malloc(sizeof(char *) * db->results_field_count);

		for(size_t j = 0; j < db->results_field_count; j++){
			db->results[i].names[j] = strdup(PQfname(res, j));
			db->results[i].values[j] = strdup(PQgetvalue(res, i, j));
		}
	}
}

static db_error_code_t db_exec_function_postgres(db_t *db, char *query, size_t params_count, va_list params){
	PGresult *res;

	if(params_count == 0){															// no params
		res = PQexec(db->context, query);
	}
	else{																			// with params
		char *query_params[params_count];
		string *values[params_count];

		// process params 
		for(size_t i = 0; i < params_count; i++){									// for each param
			db_param_t param = va_arg(params, db_param_t);
			values[i] = string_new();

			if(param.type == db_type_invalid){										// invalid type
				db->error_code = db_error_code_invalid_type;
				db_error_set_message(db, "An input param for the query was invalid", "");
				return db_error_code_invalid_type;
			}

			if(param.is_array){														// for array type
				string_cat_raw(values[i], "{");
				
				for(size_t i = 0; i < param.count; i++){

					if(i != 0)
						string_cat_raw(values[i], ",");

					switch(param.type){
						case db_type_integer:										// integer array
							string_cat_fmt(values[i], "%d", 25, ((int**)param.value)[i]);
						break;

						case db_type_bool:   										// bool array
							string_cat_fmt(values[i], "%s", 6, ((bool**)param.value)[i] ? "true" : "false");
						break;

						case db_type_float:  										// float array
							string_cat_fmt(values[i], "%f", 50, ((float**)param.value)[i]);
						break;

						case db_type_string: 										// string array
							string_cat_fmt(values[i], "%s", strlen(((char**)param.value)[i]) + 1, ((char**)param.value)[i]);
						break;

						// TODO add blob array type param
						case db_type_blob:   										// blob array
						break;

						default:
						case db_type_invalid:
						case db_type_null:
							break;
					}
				}
				string_cat_raw(values[i], "}");
			}
			else{																	// for simple type
				switch(param.type){
					case db_type_integer:											// integer
						string_cat_fmt(values[i], "%d", 25, *((int*)param.value));
					break;

					case db_type_bool:   											// bool
						string_cat_fmt(values[i], "%s", 6, *((bool*)param.value) ? "true" : "false");
					break;

					case db_type_float:  											// float
						string_cat_fmt(values[i], "%f", 50, *((float*)param.value));
					break;

					case db_type_string: 											// string
						string_cat_fmt(values[i], "%s", strlen((char*)param.value) + 1, (char*)param.value);
					break;

					// TODO add blob type param
					case db_type_blob:												// blob
					break;

					default:
					case db_type_invalid:
					case db_type_null:   											// null
						string_cat_fmt(values[i], "%s", 5, "null");
					break;
				}
			}

			query_params[i] = values[i]->raw;
		}

		res =																		// exec query 
			PQexecParams(db->context, query, params_count, NULL, (const char *const *)query_params, NULL, NULL, 0);

		for(size_t i = 0; i < params_count; i++){									// free values
			string_destroy(values[i]);
		}
	}
	
	// handle special error cases that the error map cant handle
	if(res == NULL){
		db->error_code = db_error_code_unknown;
		db_error_set_message(db, "Query response was null", PQresultErrorMessage(res));
	}
	else{
		db->error_code = db_error_code_map(db->vendor, PQresultStatus(res));
		char *msg = PQresultErrorMessage(res);

		if(db->error_code == db_error_code_fatal){									// remap code to invalid type
			if(strstr(msg, "invalid input syntax") != NULL){
				db_error_set_message(db, "Query has invalid param syntax", msg);
				db->error_code = db_error_code_invalid_type;
			}
			else if(strstr(msg, "violates unique constraint") != NULL){				// remap unique
				db_error_set_message(db, "Entry already in database", msg);
				db->error_code = db_error_code_unique_constrain_violation;
			}
			else if(strstr(msg, "too long") != NULL){				// remap unique
				db_error_set_message(db, "Invalid range for field", msg);
				db->error_code = db_error_code_invalid_range;
			}
			else if(strstr(msg, "too short") != NULL){				// remap unique
				db_error_set_message(db, "Invalid range for field", msg);
				db->error_code = db_error_code_invalid_range;
			}
		}
		else if(PQntuples(res) == 0){												// remap to zero results
			db_error_set_message(db, "Query returned 0 results", msg);
			db->error_code = db_error_code_zero_results;
		}
		else if(db->error_code == db_error_code_ok){								// complement ok message
			db_error_set_message(db, "Query executed successfully", msg);
		}
	}

	if(db->error_code == db_error_code_ok){
		db_process_entries_postgres(db, res);
	}
	
    PQclear(res);
	return db->error_code;
}

// ------------------------------------------------------------ Invalid database default

// default connection function
static db_error_code_t db_default_function_invalid(db_t *db){
	db_error_set_message(db, "Invalid database vendor", "invalid");
	db->error_code = db_error_code_invalid_db;
	db->state = db_state_invalid_db;
	return db->error_code;
}

// ------------------------------------------------------------ Maps ---------------------------------------------------------------

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
	db_clean_results(db);
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
	db_clean_results(db);

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
void db_clean_results(db_t *db){
	if(db->results_count == -1) return;

	for(size_t i = 0; i < db->results_count; i++){
		for(size_t j = 0; j < db->results_field_count; j++){
			free(db->results[i].names[j]);
			free(db->results[i].values[j]);
		}
		free(db->results[i].names);
		free(db->results[i].values);
	}
	
	db->results_count = -1;
	db->results_field_count = -1;

	free(db->results);
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
			for(size_t j = 0; j < db->results_count; j++){
				printf("| %s ", db->results[0].names[j]);

				if(j == (db->results_count - 1)){
					printf("|\n");
				}
			}	
		}

		// entry
		for(size_t j = 0; j < db->results_field_count; j++){
			printf("| %s ", db->results[i].values[j]);

			if(j == (db->results_field_count - 1)){
				printf("|\n");
			}
		}	
	}
}

// print josn
char *db_json_entries(db_t *db, bool squash_if_single){
	if(db->results_count < 0) return FIOBJ_INVALID;

	bool trail = !(squash_if_single && db->results_count == 1);

	FIOBJ json;

	if(trail)
		json = fiobj_str_new("[", 1);
	else
		json = fiobj_str_new("{", 1);

	for(size_t i = 0; i < db->results_count; i++){
		if(trail)
			fiobj_str_write(json, "{", 1);
	
		for(size_t j = 0; j < db->results_field_count; j++){
			fiobj_str_printf(json, "\"%s\":\"%s\"", db->results[i].names[j], db->results[i].values[j]);

			if(j != (db->results_field_count - 1))
				fiobj_str_write(json, ",", 1);
		}	

		if(trail)
			fiobj_str_write(json, "}", 1);

		if(i != (db->results_count - 1))
			fiobj_str_write(json, ",", 1);
	}
	if(trail)
		fiobj_str_write(json, "]", 1);
	else
		fiobj_str_write(json, "}", 1);

	char *ret = strdup(fiobj_obj2cstr(json).data);
	fiobj_free(json);
	return ret;
}

char *db_results_read(db_t *db, uint32_t entry, uint32_t field){
	if(db->results_count == -1) return NULL;
	return db->results[entry].values[field];
}