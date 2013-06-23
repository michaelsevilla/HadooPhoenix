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
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"
#define DEFAULT_DISP_NUM 10
#define START_ARRAY_SIZE 2000

enum {
	IN_WORD,
	NOT_IN_WORD,
};

// a single null-terminated word
struct wc_word {
    char* data;
    
    // necessary functions to use this as a key
    bool operator<(wc_word const& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(wc_word const& other) const {
        return strcmp(data, other.data) == 0;
    }
};

// for the sorting
//bool string_compare(const void *a, const void *b) {
bool string_compare(std::string a, std::string b) {
	// Operators are already overloaded for key comparison
	return (a.compare(b) < 0);
}


// class for the wordcounter
class WordCounter {
	char *input_data;			// A passsage from the text
	uint64_t input_len;
	std::vector<std::string> words;		// Store the list of words
public: 
	explicit WordCounter(char* _data, uint64_t length) :
		input_data(_data), input_len(length){ }

	void splitter(void) {
		char curr_letter, *curr_word;
		int state = NOT_IN_WORD;

		// For the current word
		curr_word = input_data;

	
		// Go through the file and update the count for each unique word
		for (uint64_t i = 0; i < input_len; i++) {	
			// Each word is the same, regardless of case
			curr_letter = toupper(input_data[i]);
			switch (state) {
			case IN_WORD:
				input_data[i] = curr_letter;
				if ((curr_letter < 'A' || curr_letter > 'Z') && curr_letter != '\'') {
					// End of the current word
					input_data[i] = 0;
				
					// Insert the word into the vector
					std::vector<std::string>::iterator it = words.begin();
					words.insert(it, curr_word);
					state = NOT_IN_WORD;
				}
				break;

			default: 
			case NOT_IN_WORD:
				if (curr_letter >= 'A' && curr_letter <= 'Z') {
					// Start of a new word
					curr_word = &input_data[i];
					input_data[i] = curr_letter;
					state = IN_WORD;	
				}
				break;
			}
		}
	}
	void sort() {

		// Sort the list
		std::sort(words.begin(), words.end(), string_compare);
				
	}
	void print_results(unsigned int ndisp) {
		unsigned int dn, count = 0;
		printf("\nSequential Wordcount: Results (TOP %d of %lu)\n", ndisp, words.size());

		// Determine how many values to print
		dn = std::min(ndisp, (unsigned int) words.size());

		// Print the hashtable
		printf("Printing %u results\n", dn);
		for (std::vector<std::string>::iterator it = words.begin(); count < dn; ++it) {
			std::cout << '\t'  << *it << '\n';
			count++;
		}
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

	// Memory map the file
	CHECK_ERROR((fdata = (char*) mmap(0, finfo.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0)) == NULL);

	// Prepare splitter arguments
	WordCounter wc(fdata, finfo.st_size);

	// Divide file on a word border (i.e. a space)
	printf("Sequential Sort: Calling splitter\n");
	wc.splitter();

	// Sort the results
	printf("Sequential Sort: Sorting results\n");
	wc.sort();

	// Print out results
	printf("Sequential Wordcount: Completed\n");
	wc.print_results(ndisp); 

	// Cleanup
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
	CHECK_ERROR(close(fd));

	return 0;
}
