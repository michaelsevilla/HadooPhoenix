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

#include "map_reduce.h"
#define DEFAULT_DISP_NUM 10
#define START_ARRAY_SIZE 2000
;

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
int key_compare(const void *a, const void *b) {
	const char **aa = (const char **) a;
	const char **bb = (const char **) b;

	return strcmp(*aa, *bb);
}


// class for the wordcounter
class WordCounter {
	char *input_data;			// A passsage from the text
	uint64_t input_len;
	std::string *output;	// Output reverses <k, v> so that we can sort
	uint64_t n_words, n_array_size;
	
public: 
	explicit WordCounter(char* _data, uint64_t length) :
		input_data(_data), input_len(length), 
		n_words(0), n_array_size(START_ARRAY_SIZE) {
			output = new std::string [START_ARRAY_SIZE];
		}
	~WordCounter() {
		delete [] output;
	}

	void splitter(void) {
		uint64_t i = 0;
		while (i < input_len) {
			// Not in word, iterate until the start of a new word
			while (i < input_len && (input_data[i] < '!' || input_data[i] > '~'))
				i++;
			// Start of a new word
			uint64_t start = i;
			while (i < input_len && ((input_data[i] >= '!' && input_data[i] <= '~') || input_data[i] == '\''))
				i++;
			
			if (i > start) {
				// End of the current word
				input_data[i] = 0;
		
				// Make the array bigger, if need be
				if (n_words >= n_array_size) {
					n_array_size *= 2;
					std::string *temp = new std::string [n_array_size];
					for (unsigned int i = 0; i < n_words; i++)
						temp[i] = output[i];
					delete [] output;
					output = temp;
				}
			
				// add the word into the array
				output[n_words] = input_data + start;
				n_words++;
			}
		}
		printf("# words: %lu\n", n_words);
	}
	void sort() {
		qsort(output, n_words, sizeof(std::string), key_compare);
	}
	void print_results(unsigned int ndisp) {
		unsigned int dn;
		printf("\nSequential Sort: Results\n");

		// Determine how many values to print
		dn = std::min(ndisp, (unsigned int) n_words);
	
		// Print the result
		std::string prev = "";
		uint64_t printed = 0;
		for (uint64_t i = 0; printed < dn; i++) {
			if (i > n_words)
				break;
			if (prev.compare(output[i])) {
				std::cout << "\t" << i << ": " << output[i] << "\n";
				printed++;
				prev = output[i];
			}

		}
		printf("Total: %lu\n", n_words);
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
	WordCounter *wc = new WordCounter(fdata, finfo.st_size);

	// Divide file on a word border (i.e. a space)
	printf("Sequential Sort: Calling word count\n");
	wc->splitter();

	// Sort the results
	wc->sort();

	printf("Sequential Sort: Completed\n");
	// Print out results
	wc->print_results(ndisp); 

	// Cleanup
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
	CHECK_ERROR(close(fd));
	delete wc;
	return 0;
}
