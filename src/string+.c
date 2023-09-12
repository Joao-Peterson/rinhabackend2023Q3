#include "string+.h"
#include <stdarg.h>
#include <stdio.h>

// ------------------------------------------------------------ String -------------------------------------------------------------

string *string_new_sized(size_t size){
	string *str = calloc(sizeof(char), sizeof(string));
	str->len = 0;
	str->allocated = size;
	str->managed = true;

	if(str->allocated > 0 )
		str->raw = calloc(str->allocated, sizeof(char));
	else
		str->raw = NULL;

	return str;
}

string *string_new(){
	return string_new_sized(STRING_ALLOCATION_CHUNK);
}

string *string_copy(string *str){
	if(str == NULL)
		return string_new_sized(0);

	string *new = string_new_sized(str->allocated);
	new->len = str->len;
	memcpy(new->raw, str->raw, str->len);
	return new;
}

string *string_from(char *raw){
	if(raw == NULL)
		return string_new_sized(0);
		
	size_t len = strlen(raw);
	string *str = string_new_sized(len + 1);
	str->len = len;
	memcpy(str->raw, raw, len);
	return str;
}

string *string_wrap(char *raw, bool take_ownership){
	if(raw == NULL)
		return string_new_sized(0);
		
	size_t len = strlen(raw);
	string *str = string_new_sized(0);
	str->managed = take_ownership;
	str->len = len;
	str->raw = raw;
	return str;
}

string *string_vsprint(const char *fmt, size_t max_size, va_list args){
	char *buffer = malloc(max_size);
	vsnprintf(buffer, max_size, fmt, args);
	string *str = string_from(buffer);
	free(buffer);
	return str;
}

string *string_sprint(const char *fmt, size_t max_size, ...){
	va_list args;
	va_start(args, max_size);
	string *str = string_vsprint(fmt, max_size, args);
	va_end(args);
	return str;
}

void string_destroy(string *str){
	if(str == NULL) return;
	
	if(str->managed && str->raw != NULL)
		free(str->raw);

	free(str);
	str = NULL;
}

void string_println(string *str){
	printf("%s\n", str->raw == NULL ? "" : str->raw);
}

bool string_cmp(string *a, string *b){
	if(a->raw == NULL || b->raw == NULL) return false;
	return !strcmp(a->raw, b->raw);
}

bool string_cmp_raw(string *a, char *b){
	if(a->raw == NULL || b == NULL) return false;
	return !strcmp(a->raw, b);
}

void _string_cat_raw(string *dest, char *src, size_t srclen){
	if(dest == NULL || src == NULL) return;

	size_t len = dest->len + srclen;
	if(dest->allocated == 0){
		dest->allocated = len + STRING_ALLOCATION_CHUNK;
		dest->raw = calloc(dest->allocated, sizeof(char));
		dest->len = len;
		memcpy(dest->raw, src, len);	
	}
	else{
		if(dest->allocated < (len + 1)){
			dest->allocated = len + STRING_ALLOCATION_CHUNK;
			dest->raw = realloc(dest->raw, dest->allocated);
		}

		dest->len = len;
		strncat(dest->raw, src, len);
	}
}

void string_cat_raw(string *dest, char *src){
	if(dest == NULL || src == NULL) return;

	_string_cat_raw(dest, src, strlen(src));
}

void string_cat(string *dest, string *src){
	if(dest == NULL || src == NULL) return;

	_string_cat_raw(dest, src->raw, src->len);
}

void string_cat_vfmt(string *str, const char *fmt, size_t buffer_size, va_list args){
	if(str == NULL || buffer_size == 0) return;

	string *expanded = string_vsprint(fmt, buffer_size, args);
	string_cat(str, expanded);
	string_destroy(expanded);
}

void string_cat_fmt(string *str, const char *fmt, size_t buffer_size, ...){
	va_list args;
	va_start(args, buffer_size);
	string_cat_vfmt(str, fmt, buffer_size, args);
	va_end(args);
}
