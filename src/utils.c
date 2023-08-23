#include "utils.h"

bool fiobj_str_cmp(FIOBJ fiobj, char *str) {
    const char *fiobjStr = fiobj_obj2cstr(fiobj).data;
	if(fiobjStr == NULL) return false;
    return (strcmp(fiobjStr, str) == 0);
}

bool fiobj_str_substr(FIOBJ fiobj, char *str){
    const char *fiobjStr = fiobj_obj2cstr(fiobj).data;
	if(fiobjStr == NULL) return false;
	char *sub = strstr(fiobjStr, str);
	
	return (sub != NULL);
}