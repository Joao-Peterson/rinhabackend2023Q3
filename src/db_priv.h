#ifndef _DB_HEADER_PRIV_
#define _DB_HEADER_PRIV_

#include "db.h"
#include <stdarg.h>

// ------------------------------------------------------------ Maps ---------------------------------------------------------------

// vendor name map
static const char *db_vendor_name_map(db_vendor_t vendor);
// map error codes
db_error_code_t db_error_code_map(db_vendor_t vendor, int code);

// connection functions
db_error_code_t db_connect_function_map(db_t *db);

// close db map
void db_destroy_function_map(db_t *db);

// exec query map
static db_results_t *db_exec_function_map(db_t *db, void *connection, char *query, size_t params_count, va_list params);

// ------------------------------------------------------------ Error handlng ------------------------------------------------------

// create new result object
db_results_t *db_results_new(int64_t entries, int64_t fields, db_error_code_t code, char *msg);

// create new result object
db_results_t *db_results_new_fmt(int64_t entries, int64_t fields, db_error_code_t code, char *fmt, ...);

// set result msg
void db_results_set_message(db_results_t *results, char *msg, db_vendor_t vendor, char *vendor_msg);

#endif