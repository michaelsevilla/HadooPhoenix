/*
 * Author: Michael Sevilla
 */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <unordered_map>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <parallel/algorithm>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <time.h>
#include <limits.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "stddefines.h"
#define DEFAULT_DISP_NUM	10
#define KEY_LEN			10
#define ROWID_LEN		10
#define VAL_LEN			78

#define NO_MMAP
 
enum {
    IN_KEY, 
    IN_VALUE,
    NOT_IN_WORD
};

// a single null-terminated word
struct sort_word {
    char* key;
    char* data;
    
    // necessary functions to use this as a key
    bool operator<(sort_word const& other) const {
        return strcmp(key, other.key) < 0;
    }
    bool operator==(sort_word const& other) const {
        return strcmp(key, other.key) == 0;
    }
};

// for the sorting
int key_compare(const void *a, const void *b) {
	std::pair<std::string, std::string> *pair1 = (std::pair<std::string, std::string> *) a;
	std::pair<std::string, std::string> *pair2 = (std::pair<std::string, std::string> *) b;
	std::string key1 = pair1->first;
	std::string key2 = pair2->first;


	// Operators are already overloaded for key comparison
	if (key1 < key2) return -1;
	else if (key1 > key2) return 1;
	else {
		std::string value1 = pair1->second;
		std::string value2 = pair2->second;
		return value1.compare(value2);
	}
}

// class for the sorter
class Sorter {
	uint64_t input_len;
	std::vector<std::pair<std::string, std::string>> kv_pairs;
	uint64_t n_pairs, n_array_size;
	
public: 
	explicit Sorter() :
		n_pairs(0)
		{}
	~Sorter() {
		//delete &kv_pairs;
	}

	void splitter(FILE *file) {
		char buffer[LINE_MAX];
		char key[KEY_LEN + 1], value[VAL_LEN + 1];

		// Teragen creates 10 byte keys
		n_pairs = 0;
	
		// Go through the file and update the count for each unique word
		while (fgets(buffer, LINE_MAX, file) != NULL) {
			// Get the key and value, which are at specific offsets
			CHECK_ERROR( strncpy(key, buffer, KEY_LEN) < 0);
			CHECK_ERROR( strncpy(value, buffer + KEY_LEN + ROWID_LEN, VAL_LEN) < 0);

			// Add the kv_pair to the array
			kv_pairs.push_back(std::make_pair(key, value));
			n_pairs++;
		}

	}

	void sort() {
		//qsort(kv_pairs, n_pairs, sizeof(std::pair<std::string, std::string>), key_compare);
		__gnu_parallel::sort(kv_pairs.begin(), kv_pairs.end());
	}
	void print_results(unsigned int ndisp) {
		unsigned int dn;
		printf("\nOpenMP Sort: Results:\n");

		// Determine how many values to print
		dn = std::min(ndisp, (unsigned int) n_pairs);
	
		// Print the hashtable
		int i = 0;
		for (std::vector<std::pair<std::string, std::string>>::iterator it = kv_pairs.begin();
			i < 10;
			i++) 
			std::cout << "\t" << std::get<0>(kv_pairs[i]) << " - " << std::get<1>(kv_pairs[i]) << "\n";
		printf("Total: %i\n", i);
	}
};

int main(int argc, char *argv[]) {
	char *filename, *ndisp_str;
	unsigned int ndisp;
	struct timespec begin, end, total_begin, total_end;
	FILE *file;

	get_time(total_begin);
	get_time(begin);

	if (argv[1] == NULL) {
		printf("USAGE: %s <filename> <# results>\n", argv[0]);
		exit(1);
	}

	// Initialize some variables
	filename = argv[1];
	ndisp_str = argv[2];
	CHECK_ERROR((ndisp = ((ndisp_str == NULL) ?
		DEFAULT_DISP_NUM : atoi(ndisp_str))) <= 0);

	printf("OpenMP Sort: Running...\n");
	
	// Open the file
	CHECK_ERROR((file = fopen(filename, "r")) < 0);
	//CHECK_ERROR(fstat(fd, &finfo) < 0);

#ifndef NO_MMAP
	// Memory map the file
    	printf("Memory mapping the file (with MMAP_POPULATE)\n");
#else
	//uint64_t r = 0;
	printf("Mallocing the file\n");
	//fdata = (char *)malloc (finfo.st_size);
	//CHECK_ERROR(fdata == NULL);
	//while(r < (uint64_t)finfo.st_size)
	//	r += pread (fd, fdata + r, finfo.st_size, r);
	//CHECK_ERROR(r != (uint64_t) finfo.st_size);
#endif

	get_time(end);
#ifdef TIMING
	print_time("OpenMP Sort: initialize", begin, end);
#endif


	// Prepare splitter arguments
	Sorter *sort = new Sorter();

	// Divide file on a word border (i.e. a space)
	printf("OpenMP Sort: Calling word count\n");

	get_time(begin);
	sort->splitter(file);
	get_time(end);

#ifdef TIMING
	print_time("OpenMP Sort: reading in data", begin, end);
#endif
	// Sort the results
	get_time(begin);
	sort->sort();

	get_time(end);

#ifdef TIMING
	print_time("OpenMP Sort: sort", begin, end);
#endif

	printf("OpenMP Sort: Completed\n");
	// Print out results
	sort->print_results(ndisp); 

	// Cleanup
	//delete sort;
//#ifndef NO_MMAP
//	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
//#else
//	free (fdata);
//#endif
	CHECK_ERROR(fclose(file));

	get_time(total_end);
#ifdef TIMING
	print_time("OpenMP Sort: total", total_begin, total_end);
#endif

	return 0;
}
