#ifndef _VARENV_HEADER_
#define _VARENV_HEADER_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void loadEnvVars(char *dotEnvFile){
	if(dotEnvFile == NULL)
		dotEnvFile = ".env";

	FILE *dotenv = fopen(dotEnvFile, "r");

	if(dotenv == NULL) return;

	fseek(dotenv, 0, SEEK_END);
	size_t fsize = ftell(dotenv);
	fseek(dotenv, 0, SEEK_SET);

	char *file = malloc(fsize + 1);
	fread(file, 1, fsize, dotenv);
	file[fsize] = '\0';
	fclose(dotenv);

	char *line = strtok(file, "\n\0");
	if(line == NULL){
		free(file);
		return;
	}

	while(line != NULL){
		if(*line == '\0')
			break;

		char *keyvalue = calloc(1, 256);
		strncpy(keyvalue, line, 255);
		char *key = keyvalue;
		char *value = strchr(key, '=');
		*value = '\0';
		value++;

		setenv(key, value, true);
		free(keyvalue);

		line = strtok(NULL, "\n\0");
	}
	
	free(file);
}

#endif