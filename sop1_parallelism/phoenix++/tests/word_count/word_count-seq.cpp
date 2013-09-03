/**
 * Author: Michael Sevilla
 * Edited by msevilla for a scale-up vs. scale-out study.
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

//#define NO_MMAP
 
enum {
	IN_WORD,
	NOT_IN_WORD,
};

// a single null-terminated word
struct wc_word {
    char* data;
    int count;
    
    // necessary functions to use this as a key
    bool operator<(wc_word const& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(wc_word const& other) const {
        return strcmp(data, other.data) == 0;
    }
};

// for the sorted list
int key_compare(const void *a, const void *b) {
	wc_word *w1 = (wc_word *) a;
	wc_word *w2 = (wc_word *) b;

	return strcmp(w1->data, w2->data);
}
// for the sorting
int value_compare(const void *a, const void *b) {
	wc_word *w1 = (wc_word *) a;
	wc_word *w2 = (wc_word *) b;

	if (w1->count < w2->count) return 1;
	else if (w1->count > w2->count) return -1;
	else return 0;
}
// class for the wordcounter
class WordCounter {
	char *input_data;			// A passsage from the text
	uint64_t input_len;
	wc_word *output;			// Output reverses <k, v> so that we can sort
	uint64_t n_words, n_unique_words, length;
	
	// Store the <word, count> in a hashtable for efficient access
	//std::unordered_map<std::string, int> words;
public: 
	explicit WordCounter(char* _data, uint64_t length) :
		input_data(_data), input_len(length) {}
	~WordCounter() {
		free(output);
	}

	void splitter(void) {
		char curr_letter, *curr_word;
		int state = IN_WORD;

		// For the current word
		curr_word = input_data;
		n_words = 0, n_unique_words = 0;;

		// Initialize the output array
		output = (wc_word *) calloc(START_ARRAY_SIZE, sizeof(wc_word));
		for (int i = 0; i < START_ARRAY_SIZE; i++)
			output[i].count = 0;
		length = START_ARRAY_SIZE;
	
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

					// Add key
					add_word(curr_word);
						
					// Modify the state
					state = NOT_IN_WORD;
					n_words++;
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

		// Add the last word
		//add_word(curr_word);
		//n_words++;

		printf("# words: %lu\n", n_words);
		printf("# unique words: %lu\n", n_unique_words);
	}
	void sort() {
		qsort(output, n_unique_words, sizeof(wc_word), value_compare);
	}
	void print_results(unsigned int ndisp) {
		unsigned int dn;
		printf("\nSequential Wordcount: Results (TOP %d of %lu)\n", ndisp, n_unique_words);

		// Determine how many values to print
		dn = std::min(ndisp, (unsigned int) n_unique_words);		
		for (unsigned int i = 0; i < dn; i++) 
			printf("%15s -%i\n", output[i].data, output[i].count);
		printf("Total: %lu\n", n_words);
	}
	void add_word(char *curr_word) {
		// We sort and search the array ourselves because:
		// 	1. we want to save space (not copy vlaues from an unordered map to a vector)
		// 	2. we need to get the index of an object, not the object itself
	
		// Search for the position
		//qsort(output, n_unique_words, sizeof(wc_word), key_compare);
		int pos = binary_search(curr_word);
		if (pos >= (int) n_unique_words) {
			// No elements in the list
			output[pos].data = curr_word;
			output[pos].count = 1;
			n_unique_words++;
		}
		else if (!strcmp(output[pos].data, curr_word)) {
			// We already have the word
			output[pos].count++;
		}
		else {
			// Insert word at sorted position
			memmove(&output[pos + 1], &output[pos], (n_unique_words - pos)*sizeof(wc_word));
			output[pos].data = curr_word;
			output[pos].count = 1;
			n_unique_words++;
		}

		// Extend array if it gets too small
		if (n_unique_words >= length) {
			length *= 2;
			output = (wc_word *) realloc(output, length*sizeof(wc_word));
		}
	}
	int binary_search(char *curr_word) {
		int low = -1, high = n_unique_words, mid, comparison;

		while (high - low > 1) {
			mid = (high + low) / 2;
			comparison = strcmp(curr_word, output[mid].data);
			if (comparison == 0) return mid;
			else if(comparison < 0)	high = mid;
			else low = mid;
		}

		return high;
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
	

	printf("Sequential Wordcount: Running...\n");
	
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
	WordCounter *wc = new WordCounter(fdata, finfo.st_size);

	// Divide file on a word border (i.e. a space)
	printf("Sequential Wordcount: Calling word count\n");
	wc->splitter();

	// Sort the results
	wc->sort();

	printf("Sequential Wordcount: Completed\n");
	// Print out results
	wc->print_results(ndisp); 

	// Cleanup
	delete wc;
#ifndef NO_MMAP
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
#else
	free (fdata);
#endif
	CHECK_ERROR(close(fd));

	return 0;
}
