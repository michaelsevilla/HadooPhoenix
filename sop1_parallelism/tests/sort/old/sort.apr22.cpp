

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"
#define DEFAULT_DISP_NUM 10

enum {
	IN_WORD,
	NOT_IN_WORD,
};

// a passage from the text. The input data to the Map-Reduce
struct sort_string {
    char* data;
    uint64_t len;
};

// a single null-terminated word
struct sort_word {
    char* data; 
    bool operator<(sort_word const& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(sort_word const& other) const {
        return strcmp(data, other.data) == 0;
    }
};


// a hash for the word
struct sort_word_hash {
    size_t operator()(sort_word const& key) const {
        char* h = key.data;
        uint64_t v = 14695981039346656037ULL;
        while (*h != 0)
            v = (v ^ (size_t)(*(h++))) * 1099511628211ULL;
        return v;
    }
};


// Class that must define partition(), map(), reduce(), etc.
class SortMR : public MapReduceSort<SortMR, sort_string, sort_word, uint64_t, hash_container<sort_word, uint64_t, buffer_combiner, sort_word_hash> > {
	char* data;
	uint64_t data_size;
	uint64_t chunk_size;
	uint64_t splitter_pos;
	char curr_word[LINE_MAX];
public:
	explicit SortMR(char* _data, uint64_t length, uint64_t _chunk_size) :
		data(_data), data_size(length), chunk_size(_chunk_size), splitter_pos(0) {}

	void* locate(data_type* str, uint64_t len) const {
		return str->data;
	}

	// map(): at each node, execute this function
	void map(data_type const& s, map_container& out) const {
		char curr_letter, *curr_word;
		int state = NOT_IN_WORD;
		
		// Start at the beginning of the input split
		curr_word = s.data;
	
		// Iterate through the input split and parse on spcaes and newlines
		for (uint64_t i = 0; i < s.len; i++) {
			switch(state) {
			case IN_WORD: 
				if (s.data[i] < '!' || s.data[i] > '~') {
					// End of the current word
					s.data[i] = 0;
					state = NOT_IN_WORD;
					
					// Emit the intermediate <k, v> pair
					sort_word word;
					word.data = curr_word;
					emit_intermediate(out, word, 0);
				}
				break;
			default:
			case NOT_IN_WORD:
				if (s.data[i] >= '!' && s.data[i] <= '~') {
					// Start of a new word
					curr_word = &s.data[i];				
					state = IN_WORD;
				}
				break;
			}
	
		//	// Not in word, iterate until the start of a new word
		//	while (i < s.len && (s.data[i] < '!' || s.data[i] > '~')) {
		//		printf("s.data[%d]: %c\n", i, s.data[i]);
		//		i++;
		//	}
		//	// Start of a new word
		//	uint64_t start = i;
		//	while (i < s.len && ((s.data[i] >= '!' && s.data[i] <= '~') || s.data[i] == '\'') || s.data[i] != '\n' || s.data[i] != ' ') {
		//		printf("s.data[%d]: %c\n", i, s.data[i]);
		//		i++;
		//	}
		//	
		//	if (i > start) {
		//		// End of the current word
		//		//s.data[i] = 0;
		//		strncpy(curr_word, s.data + start, i);
		//		printf("curr_word: %s\n", curr_word);
		//		
		//		// Emit the intermediate <k,v> pair
		//		sort_word word;
		//		word.data = s.data + start;
		//		emit_intermediate(out, word, 0);
		//	}
		}
	}

	// split(): split the input file (memory mapped) into words
	int split(sort_string& out)
	{
		// End of the file
		if ((uint64_t) splitter_pos >= data_size)
			return 0;
	
		// If chunk size goes over the end of the file	
		uint64_t end = std::min(splitter_pos + chunk_size, data_size);
		
		// Move end point to next word break
		while(end < data_size && 
			data[end] != ' ' && data[end] != '\t' &&
			data[end] != '\r' && data[end] != '\n')
			end++;
		
		// Trim the input data
		out.data = data + splitter_pos;
		out.len = end - splitter_pos;
		
		// Set the beginning to the front of the file
		splitter_pos = end;
		
		// Out data is valid
		return 1;
	}

	bool sort(keyval const& a, keyval const& b) const {
		return a.val < b.val || (a.val == b.val && strcmp(a.key.data, b.key.data) > 0);
	}

};

//#define NO_MMAP

int main(int argc, char *argv[]) {
	unsigned int fd, disp_num;
	char *fname, *disp_num_str, *fdata;
	struct stat finfo;

	if (argv[1] == NULL) {
		printf("USAGE: %s <filename> [ndisplay]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	printf("Sort: Running...\n");

	// Open the file
	fname = argv[1];
	disp_num_str = argv[2];
	CHECK_ERROR( (fd = open(fname, O_RDONLY)) < 0);
	CHECK_ERROR( fstat(fd, &finfo) < 0);

	// Get the number of results to display
	CHECK_ERROR( (disp_num = (disp_num_str == NULL) ? 
		DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);
	
	// Load the input file
#ifndef NO_MMAP
#ifdef MMAP_POPULATE
	// Memory map the file
	printf("Sort: Memory mapping (populate) the file\n");
	CHECK_ERROR( (fdata = (char *) mmap(0, finfo.st_size + 1, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_POPULATE, fd, 0)) == NULL);
#else
	printf("Sort: Memory mapping (no populate) the file\n");
	CHECK_ERROR( (fdata = (char *) mmap(0, finfo.st_size + 1, PROT_READ, MAP_PRIVATE, fd, 0) == NULL);
#endif
#else
    	uint64_t r = 0;
	// Read into the malloc'd space
	CHECK_ERROR( (fdata =  (char *) malloc(finfo.st_size)) == NULL);
	while (r < (uint64_t) finfo.st_size)
		r += pread (fd, fdata + r, finfo.st_size, r);
	CHECK_ERROR( r != (uint64_t) finfo.st_size);
#endif

	// Create the result buffer
	std::vector<SortMR::keyval> result;
	SortMR mapReduce(fdata, finfo.st_size, 1024*1024);
	
	// Start the job
	printf("Sort: Calling MapReduce Scheduler Sort\n");
	CHECK_ERROR( mapReduce.run(result) < 0);
	printf("Sort: MapReduce Completed\n");
	
	// Print the result
	printf("Sort: Results\n");
	unsigned int dn = std::min(disp_num, (unsigned int)result.size());
	char prev[LINE_MAX];
	CHECK_ERROR( strcpy(prev, "") < 0);
	uint64_t printed = 0;
	for (uint64_t i = 0; printed < dn; i++) { 
		char curr_word[LINE_MAX];
		if (i >= result.size()) 
			break;
		CHECK_ERROR( strcpy(curr_word, result[result.size() - 1 - i].key.data) < 0);
		if (strcmp(prev, curr_word)) {
			CHECK_ERROR( strcpy(prev, curr_word) < 0);
			printf("\t%lu: %s\n", i, curr_word);
			printed++;
		}

	}
	printf("Total: %lu\n", result.size());

	// Free memory
#ifndef NO_MMAP
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
#else
	free (fdata);
#endif
	CHECK_ERROR(close(fd) < 0);
	

	return 0;
}
