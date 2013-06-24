#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "error.h"
#include "setenvs_static.h"

/* set_cache(): pull the cache sizes from the lscpu lscpu_args
 * 	1. Write the output of `lscpu` to file lscpu.tmp
 *	2. Open lscpu.tmp and iterate through it to find the cache sizes
 */ 
void setenvs_static(char *component, char *envar, int multiplier) {
	int status, lscpu_fd;
	char *lscpu_args[3];
	char buffer[LINE_MAX];
	char *value, strvalue[LINE_MAX];
	int nvalue;
	pid_t pid;
	FILE *lscpu_stream;	

	// 1. Write the output of `lscpu` to file lscpu.tmp
	check_error((lscpu_fd = open("./.lscpu.tmp", O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO)) < 0);

	// Prepare the lscpu lscpu_args
	lscpu_args[0] = (char *) malloc((strlen("lscpu") + 1) * sizeof(char));
	strcpy(lscpu_args[0], "lscpu");
	lscpu_args[1] = 0;

	// Child executes the lscpu program
	check_error((pid = fork()) < 0);
	if (pid == 0) {
		check_error(dup2(lscpu_fd, STDOUT_FILENO) < 0);
		execvp(lscpu_args[0], lscpu_args);
	}
	else
		waitpid(pid, &status, 0);	
	
	check_error(close(lscpu_fd) < 0);
	free(lscpu_args[0]);

	// 2. Open lscpu.tmp and iterate through it to find the cache sizes
	check_error((lscpu_stream = fopen("./.lscpu.tmp", "r")) == NULL);

	while (fgets(buffer, LINE_MAX, lscpu_stream) != NULL) {
		if ((value = strstr(buffer, component)) != NULL) {
			strtok(value, ":"); 
			value = strtok(NULL, ":");
			value = strtok(value, " ");
			value = strtok(value, "\n");
			
			if (strstr(component, "cache") != NULL) {
				if (strstr(value, "K") != NULL) nvalue = multiplier * atoi(value) * 1024;
				else if (strstr(value, "M") != NULL) nvalue = multiplier * atoi(value) * 1024 * 1024;
				else if (strstr(value, "G") != NULL) nvalue = multiplier * atoi(value) * 1024 * 1024 * 1024;
				else nvalue = atoi(value);			
			}
			else if (strstr(component, "CPU")) {
				strtok(value, "-");
				nvalue = 1 + atoi(strtok(NULL, "-"));
				nvalue *= multiplier;
			}

			sprintf(strvalue, "%d", nvalue);

			check_error(setenv(envar, strvalue, 1) < 0);
		}
	}

	if (getenv(envar) == NULL) printf("\t... set %s to NULL\n", envar);
	else printf("\t... set %s to %s\n", envar, getenv(envar));

	// See if you can read it from another file
	// Modularize this code (use main for shell)
	// See if you get some global variable somewhere...
	//	- set the global variable persistently

}
