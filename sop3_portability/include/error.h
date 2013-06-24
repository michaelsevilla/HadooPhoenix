#include <stdio.h>
#include <errno.h>
#include <stdlib.h>


/* Wrapper to check for errors */
#define check_error(a)												\
	if (a) {												\
	perror("\n\n********************\nERROR at line: \t" #a "\nExiting program...\n\tSystem Msg");		\
	exit(EXIT_FAILURE);											\
}
