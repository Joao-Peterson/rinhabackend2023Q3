#ifndef _VARENV_HEADER_
#define _VARENV_HEADER_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void loadEnvVars(char *dotEnvFile){
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

	char *line = strtok(file, "\n");
	if(line == NULL){
		free(file);
		return;
	}

	line = strtok(file, "\n");
	while(line != NULL){
		if(
			(*line == '\0')
		){
			break;
		}

		char *keyvalue = calloc(1, 256);
		strncpy(keyvalue, line, 255);
		putenv(keyvalue);

		line = strtok(NULL, "\n");
	}
	
	free(file);
}

#endif