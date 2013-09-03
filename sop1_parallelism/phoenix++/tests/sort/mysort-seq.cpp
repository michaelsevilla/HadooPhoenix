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

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "stddefines.h"
#define DEFAULT_DISP_NUM 10
#define START_ARRAY_SIZE 2000

//#define NO_MMAP
 
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
	char *input_data;			// A passsage from the text
	uint64_t input_len;
	std::pair<std::string, std::string> *kv_pairs;	// Output reverses <k, v> so that we can sort
	uint64_t n_pairs, n_array_size;
	
public: 
	explicit Sorter(char* _data, uint64_t length) :
		input_data(_data), input_len(length),
		n_pairs(0), n_array_size(START_ARRAY_SIZE) {
		kv_pairs = new std::pair<std::string, std::string> [START_ARRAY_SIZE];
	}
	~Sorter() {
		delete [] kv_pairs;
	}

	void splitter(void) {
		char *curr_word, *curr_key, curr_ltr;
		int state = IN_KEY;

		// For the current word
		curr_word = input_data;
		curr_key = input_data;

		// Teragen creates 10 byte keys
		int nchar = 0;
		n_pairs = 0;
	
		// Go through the file and update the count for each unique word
		for (uint64_t i = 0; i < input_len; i++) {	
			// Each word is the same, regardless of case
			curr_ltr = input_data[i];
			switch (state) {
			case IN_KEY:
				if (nchar == 0)
					curr_word = &input_data[i];
				if (nchar >= 10) {
					// End of the current word
					input_data[i] = 0;
					state = NOT_IN_WORD;
					curr_key = curr_word;
					nchar = 0;		
				}
				else
					nchar ++;
				break;
			case IN_VALUE:
				if (curr_ltr == '\n') {
					// The values end in '\r\n'
					input_data[i] = 0;
					input_data[i-1] = 0;

					// Add key-value
					add_kv(curr_key, curr_word);
					
					// Modify the state
					state = IN_KEY;
				}
				break;
			default:
			case NOT_IN_WORD:
				if (curr_ltr >= '!' && curr_ltr <= '~') {
					// Start of a new word
					curr_word = &input_data[i];
					state = IN_VALUE;
				}
				break;
			}
		}

		// Add the last word
		//add_kv(curr_key, curr_word);
		//n_pairs++;
	}

	void sort() {
		qsort(kv_pairs, n_pairs, sizeof(std::pair<std::string, std::string>), key_compare);
	}
	void print_results(unsigned int ndisp) {
		unsigned int dn;
		printf("\nSequential Sort: Results:\n");

		// Determine how many values to print
		dn = std::min(ndisp, (unsigned int) n_pairs);
	
		// Print the hashtable
		for (unsigned int i = 0; i < dn; i++) 
			std::cout << "\t" << std::get<0>(kv_pairs[i]) << " - " << std::get<1>(kv_pairs[i]) << "\n";
		printf("Total: %lu\n", n_pairs);
	}
	void add_kv(char *key, char *value) {
		// Make the array bigger, if need be
		if (n_pairs >= n_array_size) {
			n_array_size *= 2;
			std::pair<std::string, std::string> *temp = new std::pair<std::string, std::string> [n_array_size];
			for (unsigned int i = 0; i < n_pairs; i++) 
				temp[i] = kv_pairs[i];
			delete [] kv_pairs;
			kv_pairs = temp;
		}
		
		// Add the word into the array
		kv_pairs[n_pairs].first = key;
		kv_pairs[n_pairs].second = value;

		n_pairs++;
	}

};

int main(int argc, char *argv[]) {
	char *filename, *ndisp_str, *fdata;
	unsigned int fd, ndisp;
	struct stat finfo;

	if (argv[1] == NULL) {
		printf("USAGE: %s <filename> <# results>\n", argv[0]);
		exit(1);
	}

	// Initialize some variables
	filename = argv[1];
	ndisp_str = argv[2];
	CHECK_ERROR((ndisp = ((ndisp_str == NULL) ?
		DEFAULT_DISP_NUM : atoi(ndisp_str))) <= 0);
	

	printf("Sequential Sort: Running...\n");
	
	// Open the file
	CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
	CHECK_ERROR(fstat(fd, &finfo) < 0);

#ifndef NO_MMAP
	// Memory map the file
    	printf("Memory mapping the file (with MMAP_POPULATE)\n");
	CHECK_ERROR((fdata = (char*) mmap(0, finfo.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0)) == NULL);
#else
	uint64_t r = 0;
	printf("Mallocing the file\n");
	fdata = (char *)malloc (finfo.st_size);
	CHECK_ERROR(fdata == NULL);
	while(r < (uint64_t)finfo.st_size)
		r += pread (fd, fdata + r, finfo.st_size, r);
	CHECK_ERROR(r != (uint64_t) finfo.st_size);
#endif

	// Prepare splitter arguments
	Sorter *sort = new Sorter(fdata, finfo.st_size);

	// Divide file on a word border (i.e. a space)
	printf("Sequential Sort: Calling word count\n");
	sort->splitter();

	// Sort the results
	sort->sort();

	printf("Sequential Sort: Completed\n");
	// Print out results
	sort->print_results(ndisp); 

	// Cleanup
	delete sort;
#ifndef NO_MMAP
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
#else
	free (fdata);
#endif
	CHECK_ERROR(close(fd));

	return 0;
}
