#ifndef _STRING_HEADER_
#define _STRING_HEADER_

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <regex.h>

// ------------------------------------------------------------ Defines ------------------------------------------------------------

#define STRING_ALLOCATION_CHUNK 100

// ------------------------------------------------------------ String -------------------------------------------------------------

typedef struct{
	bool managed;
	char *raw;
	size_t len;
	size_t allocated;
}string;

string *string_new();

string *string_new_sized(size_t size);

string *string_copy(string *string);

string *string_from(char *string);

string *string_wrap(char *string, bool take_ownership);

string *string_vsprint(const char *fmt, size_t max_size, va_list args);

string *string_sprint(const char *fmt, size_t max_size, ...);

void string_destroy(string *string);

void string_println(string *string);

bool string_cmp_raw(string *a, char *b);

bool string_cmp(string *a, string *b);

void string_cat_raw(string *dest, char *src);

void string_cat(string *dest, string *src);

void string_cat_vfmt(string *string, const char *fmt, size_t buffer_size, va_list args);

void string_cat_fmt(string *string, const char *fmt, size_t buffer_size, ...);

#endif
