#include <stdlib.h>
#include <stdio.h>
#include "facil.io/http.h"
#include "inc/varenv.h"

int main(int argq, char **argv, char **envp){

	// get env
	loadEnvVars(NULL);
	char *port = getenv("SERVER_PORT");

	char *hello = getenv("HELLO");
	printf("var: %s\n", port);
	printf("var: %s\n", hello);
	
	return 0;
}