#include "db.h"
#include "db_priv.h"
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>
#include "string+.h"
#include "../facil.io/fiobj.h"
#include "../facil.io/fiobj_json.h"

// ------------------------------------------------------------ Postgres -----------------------------------------------------------

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

// connection function
static db_error_code_t db_connect_function_postgres(db_t *db){

	if(!PQisthreadsafe())
		return db_error_code_invalid_db;

	db->state = db_state_not_connected;

	const char *keys[] = {
		"host",
		"port",
		"dbname",
		"user",
		"password",
		NULL
	};

	char *values[] = {
		db->host,
		db->port,
		db->database,
		db->user,
		db->password,
		NULL
	};

	// check first as sync
	PGconn *conn = PQconnectdbParams((const char *const *)keys, (const char *const *)values, 0);
	
    if (PQstatus(conn) == CONNECTION_BAD) {
		// db_error_set_message(db, "Connection to database failed", PQerrorMessage(conn));
		db->state = db_state_failed_connection;
		PQfinish(conn);
		return db_error_code_connection_error;
    }

	// create other connections
	PGconn **connections = calloc(db->context.connections_count, sizeof(PGconn*));
	db->context.connections = connections;
	db->context.available_connection = 0;
	connections[0] = conn;

	for(size_t i = 1; i < db->context.connections_count; i++){
		connections[i] = PQconnectStartParams((const char *const *)keys, (const char *const *)values, 0);

		if(connections[i] == NULL){													// on mass creating of connections, if error, free all created ones
			for(size_t j = i - 1; j >= 0; j--){
				PQfinish(connections[i]);
			}

			db->state = db_state_failed_connection;
			return db_state_failed_connection;
		}
	}

	db->state = db_state_connecting;
	return db_error_code_ok;
}

// try and get a connection
static inline PGconn *db_request_conn_postgres(db_t *db){
	if(db->state != db_state_connected) return NULL;
	if(db->context.available_connection >= db->context.connections_count) return NULL;

	pthread_mutex_lock(&(db->context.connections_lock));
	
	PGconn **conns = db->context.connections;
	PGconn *available_connection = conns[db->context.available_connection];
	db->context.available_connection++;

	pthread_mutex_unlock(&(db->context.connections_lock));

	return available_connection;
}

// return used connection
static inline void db_return_conn_postgres(db_t *db, PGconn *conn){
	if(db->context.available_connection == 0) return;
	
	PGconn **conns = db->context.connections;
	
	pthread_mutex_lock(&(db->context.connections_lock));

	db->context.available_connection--;
	conns[db->context.available_connection] = conn; 

	pthread_mutex_unlock(&(db->context.connections_lock));
}

// stat connection
static db_state_t db_stat_function_postgres(db_t *db){
	PGconn **connections = db->context.connections;

	if(connections != NULL){

		bool all_ok = true;

		for(size_t i = 0; i < db->context.connections_count; i++){
			if(connections[i] == NULL)												// any null cionnection is game over
				return db_state_invalid_db;

			switch(PQconnectPoll(connections[i])){
				case PGRES_POLLING_FAILED:											// if bad return failed
					db->state = db_state_failed_connection;
					return db_state_failed_connection;
					break;

				case PGRES_POLLING_OK:												// when ok just continue to verify others
					break;

				default:															// if any state other than ok or bad, then current status remain
					all_ok = false;
					break;
			}
		}

		if(all_ok){
			db->state = db_state_connected;											// every connection was ok, then connected
			return db_state_connected;
		}
		else{
			return db->state;														// if any other postgres state then return current db state
		}
	}
	else{
		return db_state_invalid_db;													// no connections = game over
	}
}

// close db connection
static void db_destroy_function_postgres(db_t *db){
	PGconn **connections = db->context.connections;

	if(connections != NULL){
		for(size_t i = 0; i < db->context.connections_count; i++){
			if(connections[i] != NULL)
				PQfinish(connections[i]);
		}
	}

	free(db->context.connections);
}

// postgres oid from pg_types table
typedef enum{
	oid_int2vector = 22,
	oid_oidvector = 30,
	oid_bool = 16,
	oid_pg_type = 71,
	oid_pg_attribute = 75,
	oid_pg_proc = 81,
	oid_pg_class = 83,
	oid_date = 1082,
	oid_time = 1083,
	oid_timestamp = 1114,
	oid_timestamptz = 1184,
	oid_timetz = 1266,
	oid_point = 600,
	oid_lseg = 601,
	oid_path = 602,
	oid_box = 603,
	oid_polygon = 604,
	oid_line = 628,
	oid_circle = 718,
	oid_inet = 869,
	oid_cidr = 650,
	oid_int8 = 20,
	oid_int2 = 21,
	oid_int4 = 23,
	oid_regproc = 24,
	oid_oid = 26,
	oid_float4 = 700,
	oid_float8 = 701,
	oid_money = 790,
	oid_numeric = 1700,
	oid_regprocedure = 2202,
	oid_regoper = 2203,
	oid_regoperator = 2204,
	oid_regclass = 2205,
	oid_regcollation = 4191,
	oid_regtype = 2206,
	oid_regrole = 4096,
	oid_regnamespace = 4089,
	oid_regconfig = 3734,
	oid_regdictionary = 3769,
	oid_pg_ddl_command = 32,
	oid_record = 2249,
	oid_cstring = 2275,
	oid_any = 2276,
	oid_anyarray = 2277,
	oid_void = 2278,
	oid_trigger = 2279,
	oid_event_trigger = 3838,
	oid_language_handler = 2280,
	oid_internal = 2281,
	oid_anyelement = 2283,
	oid_anynonarray = 2776,
	oid_anyenum = 3500,
	oid_fdw_handler = 3115,
	oid_index_am_handler = 325,
	oid_tsm_handler = 3310,
	oid_table_am_handler = 269,
	oid_anyrange = 3831,
	oid_anycompatible = 5077,
	oid_anycompatiblearray = 5078,
	oid_anycompatiblenonarray = 5079,
	oid_anycompatiblerange = 5080,
	oid_anymultirange = 4537,
	oid_anycompatiblemultirange = 4538,
	oid_int4range = 3904,
	oid_numrange = 3906,
	oid_tsrange = 3908,
	oid_tstzrange = 3910,
	oid_daterange = 3912,
	oid_int8range = 3926,
	oid_int4multirange = 4451,
	oid_nummultirange = 4532,
	oid_tsmultirange = 4533,
	oid_tstzmultirange = 4534,
	oid_datemultirange = 4535,
	oid_int8multirange = 4536,
	oid_name = 19,
	oid_text = 25,
	oid_bpchar = 1042,
	oid_varchar = 1043,
	oid_interval = 1186,
	oid_bytea = 17,
	oid_tid = 27,
	oid_xid = 28,
	oid_cid = 29,
	oid_json = 114,
	oid_xml = 142,
	oid_xid8 = 5069,
	oid_macaddr = 829,
	oid_macaddr8 = 774,
	oid_aclitem = 1033,
	oid_refcursor = 1790,
	oid_uuid = 2950,
	oid_pg_lsn = 3220,
	oid_tsvector = 3614,
	oid_gtsvector = 3642,
	oid_tsquery = 3615,
	oid_jsonb = 3802,
	oid_jsonpath = 4072,
	oid_txid_snapshot = 2970,
	oid_pg_snapshot = 5038,
	oid_bit = 1560,
	oid_varbit = 1562,
	oid_unknown = 705,
	oid_char = 18,
	oid_pg_node_tree = 194,
	oid_pg_ndistinct = 3361,
	oid_pg_dependencies = 3402,
	oid_pg_mcv_list = 5017,
	oid_pg_brin_bloom_summary = 4600,
	oid_pg_brin_minmax_multi_summary = 4601,
	oid__bool = 1000,
	oid__bytea = 1001,
	oid__char = 1002,
	oid__name = 1003,
	oid__int8 = 1016,
	oid__int2 = 1005,
	oid__int2vector = 1006,
	oid__int4 = 1007,
	oid__regproc = 1008,
	oid__text = 1009,
	oid__oid = 1028,
	oid__tid = 1010,
	oid__xid = 1011,
	oid__cid = 1012,
	oid__oidvector = 1013,
	oid__pg_type = 210,
	oid__pg_attribute = 270,
	oid__pg_proc = 272,
	oid__pg_class = 273,
	oid__json = 199,
	oid__xml = 143,
	oid__xid8 = 271,
	oid__point = 1017,
	oid__lseg = 1018,
	oid__path = 1019,
	oid__box = 1020,
	oid__polygon = 1027,
	oid__line = 629,
	oid__float4 = 1021,
	oid__float8 = 1022,
	oid__circle = 719,
	oid__money = 791,
	oid__macaddr = 1040,
	oid__inet = 1041,
	oid__cidr = 651,
	oid__macaddr8 = 775,
	oid__aclitem = 1034,
	oid__bpchar = 1014,
	oid__varchar = 1015,
	oid__date = 1182,
	oid__time = 1183,
	oid__timestamp = 1115,
	oid__timestamptz = 1185,
	oid__interval = 1187,
	oid__timetz = 1270,
	oid__bit = 1561,
	oid__varbit = 1563,
	oid__numeric = 1231,
	oid__refcursor = 2201,
	oid__regprocedure = 2207,
	oid__regoper = 2208,
	oid__regoperator = 2209,
	oid__regclass = 2210,
	oid__regcollation = 4192,
	oid__regtype = 2211,
	oid__regrole = 4097,
	oid__regnamespace = 4090,
	oid__uuid = 2951,
	oid__pg_lsn = 3221,
	oid__tsvector = 3643,
	oid__gtsvector = 3644,
	oid__tsquery = 3645,
	oid__regconfig = 3735,
	oid__regdictionary = 3770,
	oid__jsonb = 3807,
	oid__jsonpath = 4073,
	oid__txid_snapshot = 2949,
	oid__pg_snapshot = 5039,
	oid__int4range = 3905,
	oid__numrange = 3907,
	oid__tsrange = 3909,
	oid__tstzrange = 3911,
	oid__daterange = 3913,
	oid__int8range = 3927,
	oid__int4multirange = 6150,
	oid__nummultirange = 6151,
	oid__tsmultirange = 6152,
	oid__tstzmultirange = 6153,
	oid__datemultirange = 6155,
	oid__int8multirange = 6157,
	oid__cstring = 1263,
}postgres_oid_types_t;

// map Oid to data types
static db_type_t db_type_map_postgres(Oid oid){
	switch(oid){
		// typcategory A, Array types 
		case oid_int2vector:
		case oid_oidvector:
		case oid__name:
			return db_type_string_array;

		case oid__bool:
			return db_type_bool_array;

		case oid__bytea:
		case oid__char:
		case oid__int8:
		case oid__int2:
		case oid__int4:
		case oid__oid:
		case oid__tid:
		case oid__xid:
		case oid__cid:
			return db_type_integer_array;

		case oid__float4:
		case oid__float8:
		case oid__numeric:
		case oid__money:
			return db_type_float_array;

		case oid__int2vector:
		case oid__regproc:
		case oid__text:
		case oid__oidvector:
		case oid__pg_type:
		case oid__pg_attribute:
		case oid__pg_proc:
		case oid__pg_class:
		case oid__json:
		case oid__xml:
		case oid__xid8:
		case oid__point:
		case oid__lseg:
		case oid__path:
		case oid__box:
		case oid__polygon:
		case oid__line:
		case oid__circle:
		case oid__macaddr:
		case oid__inet:
		case oid__cidr:
		case oid__macaddr8:
		case oid__aclitem:
		case oid__bpchar:
		case oid__varchar:
		case oid__date:
		case oid__time:
		case oid__timestamp:
		case oid__timestamptz:
		case oid__interval:
		case oid__timetz:
		case oid__bit:
		case oid__varbit:
		case oid__refcursor:
		case oid__regprocedure:
		case oid__regoper:
		case oid__regoperator:
		case oid__regclass:
		case oid__regcollation:
		case oid__regtype:
		case oid__regrole:
		case oid__regnamespace:
		case oid__uuid:
		case oid__pg_lsn:
		case oid__tsvector:
		case oid__gtsvector:
		case oid__tsquery:
		case oid__regconfig:
		case oid__regdictionary:
		case oid__jsonb:
		case oid__jsonpath:
		case oid__txid_snapshot:
		case oid__pg_snapshot:
		case oid__int4range:
		case oid__numrange:
		case oid__tsrange:
		case oid__tstzrange:
		case oid__daterange:
		case oid__int8range:
		case oid__int4multirange:
		case oid__nummultirange:
		case oid__tsmultirange:
		case oid__tstzmultirange:
		case oid__datemultirange:
		case oid__int8multirange:
		case oid__cstring:
			return db_type_string_array;

		// typcategory B, Boolean types 
		case oid_bool:
			return db_type_bool;

		// typcategory C, Composite types 
		case oid_pg_type:
		case oid_pg_attribute:
		case oid_pg_proc:
		case oid_pg_class:
			return db_type_string;

		// typcategory D, Date/time types 
		case oid_date:
		case oid_time:
		case oid_timestamp:
		case oid_timestamptz:
		case oid_timetz:
			return db_type_string;

		// typcategory E, Enum types 
		// case :

		// typcategory G, Geometric types 
		case oid_point:
		case oid_lseg:
		case oid_path:
		case oid_box:
		case oid_polygon:
		case oid_line:
		case oid_circle:
			return db_type_string;

		// typcategory I, Network address types 
		case oid_inet:
		case oid_cidr:
			return db_type_string;

		// typcategory N, Numeric types 
		case oid_int8:
		case oid_int2:
		case oid_int4:
		case oid_regproc:
		case oid_oid:
			return db_type_integer;
		
		case oid_float4:
		case oid_float8:
		case oid_money:
		case oid_numeric:
			return db_type_float;

		case oid_regprocedure:
		case oid_regoper:
		case oid_regoperator:
		case oid_regclass:
		case oid_regcollation:
		case oid_regtype:
		case oid_regrole:
		case oid_regnamespace:
		case oid_regconfig:
		case oid_regdictionary:
			return db_type_integer;

		// typcategory P, Pseudo-types 
		case oid_pg_ddl_command:
		case oid_record:
		case oid_cstring:
		case oid_any:
		case oid_anyarray:
		case oid_void:
		case oid_trigger:
		case oid_event_trigger:
		case oid_language_handler:
		case oid_internal:
		case oid_anyelement:
		case oid_anynonarray:
		case oid_anyenum:
		case oid_fdw_handler:
		case oid_index_am_handler:
		case oid_tsm_handler:
		case oid_table_am_handler:
		case oid_anyrange:
		case oid_anycompatible:
		case oid_anycompatiblearray:
		case oid_anycompatiblenonarray:
		case oid_anycompatiblerange:
		case oid_anymultirange:
		case oid_anycompatiblemultirange:
			return db_type_string;

		// typcategory R, Range types 
		case oid_int4range:
		case oid_numrange:
		case oid_tsrange:
		case oid_tstzrange:
		case oid_daterange:
		case oid_int8range:
		case oid_int4multirange:
		case oid_nummultirange:
		case oid_tsmultirange:
		case oid_tstzmultirange:
		case oid_datemultirange:
		case oid_int8multirange:
			return db_type_string;

		// typcategory S, String types 
		case oid_name:
		case oid_text:
		case oid_bpchar:
		case oid_varchar:
			return db_type_string;

		// typcategory T, Timespan types 
		case oid_interval:
			return db_type_string;

		// typcategory U, User-defined types 
		case oid_bytea:
		case oid_tid:
		case oid_xid:
		case oid_cid:
		case oid_json:
		case oid_xml:
		case oid_xid8:
		case oid_macaddr:
		case oid_macaddr8:
		case oid_aclitem:
		case oid_refcursor:
		case oid_uuid:
		case oid_pg_lsn:
		case oid_tsvector:
		case oid_gtsvector:
		case oid_tsquery:
		case oid_jsonb:
		case oid_jsonpath:
		case oid_txid_snapshot:
		case oid_pg_snapshot:
			return db_type_string;

		// typcategory V, Bit-string types 
		case oid_bit:
		case oid_varbit:
			return db_type_string;

		// typcategory X, unknown type 
		case oid_unknown:
			return db_type_invalid;

		// typcategory Z, Internal-use types 
		case oid_char:
		case oid_pg_node_tree:
		case oid_pg_ndistinct:
		case oid_pg_dependencies:
		case oid_pg_mcv_list:
		case oid_pg_brin_bloom_summary:
		case oid_pg_brin_minmax_multi_summary:
			return db_type_string;
	}

	return db_type_invalid;
}

// process entries
static void db_process_entries_postgres(db_results_t *results, PGresult *res){
	results->entries_count = PQntuples(res);
	results->fields_count = PQnfields(res);
	db_type_t types[results->fields_count];

	results->entries = malloc(sizeof(db_entry_t) * results->entries_count);
	results->fields = malloc(sizeof(db_entry_t) * results->fields_count);

	// fields names and types
	for(size_t j = 0; j < results->fields_count; j++){
		results->fields[j] = strdup(PQfname(res, j));
		types[j] = db_type_map_postgres(PQftype(res, j));
	}

	// result values
	for(size_t i = 0; i < results->entries_count; i++){
		results->entries[i] = malloc(sizeof(db_entry_t) * results->fields_count);

		// for every columns/field
		for(size_t j = 0; j < results->fields_count; j++){
			db_entry_t entry = {
				.value = NULL,
				.count = 0,
				.size = 0,
				.type = types[j]
			};

			if(PQgetisnull(res, i, j)){												// if null value
				entry.type = db_type_null;
				results->entries[i][j] = entry;
				continue;
			}

			switch(types[j]){
				// unexpected values
				default:
				case db_type_null:
				case db_type_invalid:
					break;

				case db_type_bool:
					entry.size = sizeof(bool);
					entry.value = malloc(entry.size);
					*((bool*)entry.value) = (bool)strcmp(PQgetvalue(res, i, j), "false");
					break;

				case db_type_integer:
					entry.size = sizeof(int);
					entry.value = malloc(entry.size);
					*((int*)entry.value) = (int)strtol(PQgetvalue(res, i, j), NULL, 10);
					break;

				case db_type_float:
					entry.size = sizeof(float);
					entry.value = malloc(entry.size);
					*((float*)entry.value) = (float)strtof(PQgetvalue(res, i, j), NULL);
					break;
					
				case db_type_string:
					entry.size = PQgetlength(res, i, j);
					entry.value = strdup(PQgetvalue(res, i, j));
					break;

				// case db_type_blob:
				
				case db_type_integer_array:
				{
					size_t elem = 0;
					char *array = strdup(PQgetvalue(res, i, j));
					char *cursor = array;
					char *current_value;

					// count values
					while(cursor != NULL){
						if(*cursor == ',' || *cursor == '{')
							entry.count++;

						cursor++;	
					}

					entry.size = sizeof(int);
					entry.value = malloc(entry.size * entry.count);
					cursor = array;
					
					// get values
					while(cursor != NULL){
						if(*cursor == '{'){
							cursor++;
							current_value = cursor;
						}
						else if(*cursor == '}'){
							*cursor = '\0';
							*((int**)entry.value)[elem] = (int)strtol(current_value, NULL, 10);
							break;
						}
						else if(*cursor == ','){
							*cursor = '\0';
							*((int**)entry.value)[elem] = (int)strtol(current_value, NULL, 10);
							elem++;
							cursor++;
						}
						else{
							cursor++;
						}
					}
					
					free(array);
				}
				break;

				case db_type_string_array:
				{
					size_t elem = 0;
					char *array = strdup(PQgetvalue(res, i, j));
					char *cursor = array;
					char *current_value;

					// count values
					while(*cursor != '\0'){
						if(*cursor == ',' || *cursor == '{')
							entry.count++;

						cursor++;	
					}

					entry.size = sizeof(char*);
					entry.value = malloc(entry.size * entry.count);
					cursor = array;
					
					// get values
					while(*cursor != '\0'){
						if(*cursor == '{'){
							cursor++;
							current_value = cursor;
						}
						else if(*cursor == '}'){
							*cursor = '\0';
							((char**)entry.value)[elem] = strdup(current_value);
							break;
						}
						else if(*cursor == ','){
							*cursor = '\0';
							((char**)entry.value)[elem] = strdup(current_value);
							elem++;
							cursor++;
							current_value = cursor;
						}
						else{
							cursor++;
						}
					}
					
					free(array);
				}
				break;

				case db_type_bool_array:
					break;
				case db_type_float_array:
					break;
				case db_type_blob_array:
					break;
			}

			results->entries[i][j] = entry;
		}
	}
}

static db_results_t *db_exec_function_postgres(db_t *db, void *connection, char *query, size_t params_count, va_list params){
	PGresult *res;
	PGconn *conn = (PGconn*)connection;

	if(params_count == 0){															// no params
		res = PQexec(conn, query);
	}
	else{																			// with params
		char *query_params[params_count];
		string *values[params_count];

		// process params 
		for(size_t i = 0; i < params_count; i++){									// for each param
			db_param_t param = va_arg(params, db_param_t);
			values[i] = string_new();

			if(param.type == db_type_invalid){										// invalid type
				return db_results_new(0, 0, db_error_code_invalid_type, "An input param for the query was invalid");
			}

			if(param.is_array){														// for array type
				string_cat_raw(values[i], "{");
				
				for(size_t j = 0; j < param.count; j++){

					if(j != 0)
						string_cat_raw(values[i], ",");

					switch(param.type){
						case db_type_integer_array:									// integer array
							string_cat_fmt(values[i], "%d", 25, ((int**)param.value)[j]);
						break;

						case db_type_bool_array:   									// bool array
							string_cat_fmt(values[i], "%s", 6, ((bool**)param.value)[j] ? "true" : "false");
						break;

						case db_type_float_array:  									// float array
							string_cat_fmt(values[i], "%f", 50, ((float**)param.value)[j]);
						break;

						case db_type_string_array: 									// string array
							string_cat_fmt(values[i], "%s", strlen(((char**)param.value)[j]) + 1, ((char**)param.value)[j]);
						break;

						// TODO add blob array type param
						// case db_type_blob_array:   									// blob array
						// break;

						default:
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
			PQexecParams(conn, query, params_count, NULL, (const char *const *)query_params, NULL, NULL, 0);

		for(size_t i = 0; i < params_count; i++){									// free values
			string_destroy(values[i]);
		}
	}
	
	db_results_t *results = db_results_new(0, 0, db_error_code_ok, NULL);

	// handle special error cases that the error map cant handle
	if(res == NULL){
		db_results_set_message(results, "Query response was null", db->vendor, PQresultErrorMessage(res));
	}
	else{
		db_error_code_t code = db_error_code_map(db->vendor, PQresultStatus(res));
		char *msg = PQresultErrorMessage(res);

		if(code == db_error_code_fatal){											// remap code to invalid type
			if(strstr(msg, "invalid input syntax") != NULL){
				results->code = db_error_code_invalid_type;
				db_results_set_message(results, "Query has invalid param syntax", db->vendor, msg);
			}
			else if(strstr(msg, "violates unique constraint") != NULL){				// remap unique
	   			results->code = db_error_code_unique_constrain_violation;
				db_results_set_message(results, "Entry already in database", db->vendor, msg);
			}
			else if(strstr(msg, "too long") != NULL){								// remap unique
	   			results->code = db_error_code_invalid_range;
				db_results_set_message(results, "Invalid range for field", db->vendor, msg);
			}
			else if(strstr(msg, "too short") != NULL){								// remap unique
	   			results->code = db_error_code_invalid_range;
				db_results_set_message(results, "Invalid range for field", db->vendor, msg);
			}
			else{
				db_results_set_message(results, "Fatal error", db->vendor, msg);
			}
		}
		else if(code == db_error_code_ok){								// complement ok message
			db_results_set_message(results, "Query executed successfully", db->vendor, msg);
		}
	}

	if(results->code == db_error_code_ok){
		db_process_entries_postgres(results, res);
	}
	
    PQclear(res);
	return results;
}