// Test program for the get hardware library
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stddefines.h"
#include "map_reduce.h"
#include "setenvs_static.h"

int main(int argc, char *argv[]) {
	int option_index = 0;

	printf("Querying the hardware to enhance portability\n");
	if (argc < 3) {
		printf("\n\tUSAGE: %s --<option> <arg1> [--<option> <arg2> ...] \n", argv[0]);
		printf("\toptions - \n\t--l1_size\n\t--l2_size\n\t--l3_size\n\t--num_cpus\n");
		exit(EXIT_SUCCESS);
	}

	static struct option long_options[] = {
		{"l1_size",	required_argument, 0, 0},
		{"l2_size", 	required_argument, 0, 0},
		{"l3_size", 	required_argument, 0, 0},
		{"num_cpus", 	required_argument, 0, 0},
		{0, 0, 0, 0}
	};

	printf("Setting environment variables...\n");
	while (getopt_long_only(argc, argv, "", long_options, &option_index) != EOF) {
		const char *component = long_options[option_index].name;
		char *envar = optarg;
		if (!strcmp(component, "l1_size") )
			setenvs_static("L1d cache", envar);
		else if (!strcmp(component, "l2_size"))
			setenvs_static("L2 cache", envar);
		else if (!strcmp(component, "l3_size"))
			setenvs_static("L3 cache", envar);
		else if (!strcmp(component, "num_cpus"))
			setenvs_static("On-line CPU(s) list", envar);
	}
	return 0;
}
