#ifndef _DB_HEADER_PRIV_
#define _DB_HEADER_PRIV_

#include "db.h"

// ------------------------------------------------------------ Maps ---------------------------------------------------------------

// vendor name map
static const char *db_vendor_name_map(db_vendor_t vendor);
// map error codes
db_error_code_t db_error_code_map(db_vendor_t vendor, int code);

typedef db_error_code_t (*db_connect_function_t)(db_t *);

// connection functions
db_error_code_t db_connect_function_map(db_t *db);
// close db map
void db_close_function_map(db_t *db);

// exec query map
static db_error_code_t db_exec_function_map(db_t *db, char *query, size_t params_count, va_list params);

// ------------------------------------------------------------ Error handlng ------------------------------------------------------

void db_error_set_message(db_t *db, char *msg, char *vendor_msg);

// ------------------------------------------------------------ State handling -----------------------------------------------------

#define db_state_depends_connection(db) if(db->state != db_state_connected){ \
	db->error_code = db_error_code_connection_error; \
	db_error_set_message(db, "This call needs an active connection to the database", (char *)__func__);} \
	if(db->state != db_state_connected) \
	return db->error_code

#endif