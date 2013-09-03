// Modified by Michael sevilla
// April 22, 2013

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"
#define DEFAULT_DISP_NUM 10

// Global variables
#define NO_MMAP
//#define TBB
unsigned long filesize = 0;

enum {
    IN_KEY,
    IN_VALUE,
    NOT_IN_WORD
};

// a passage from the text. The input data to the MapReduce job
struct sort_data {
    char* data;
    uint64_t len;
};

// a single null-terminated word
struct sort_word {
    char* key; 
    char* value;
    bool operator<(sort_word const& other) const {
        //return strcmp(data, other.data) < 0;
        return strcmp(key, other.key) < 0;
    }
    bool operator==(sort_word const& other) const {
        //return strcmp(data, other.data) == 0;
        return strcmp(key, other.key) == 0;
    }
};


// a hash for the word
struct sort_word_hash {
    size_t operator()(sort_word const& key) const {
        char* h = key.key;
        uint64_t v = 14695981039346656037ULL;
        while (*h != 0)
            v = (v ^ (size_t)(*(h++))) * 1099511628211ULL;
        return v;
    }
};

// Class that must define partition(), map(), reduce(), etc.
#ifdef MUST_USE_FIXED_HASH
class SortMR : public MapReduceSort<SortMR, sort_data, sort_word, uint64_t, fixed_hash_container<sort_word, uint64_t, sum_combiner, 32768, sort_word_hash
#else
class SortMR : public MapReduceSort<SortMR, sort_data, sort_word, uint64_t, hash_container<sort_word, uint64_t, sum_combiner, sort_word_hash 
// This array probably needs to be bigger :3
//class SortMR : public MapReduce<SortMR, sort_data, sort_word, uint64_t, common_array_container<sort_word, uint64_t, one_combiner, 1000
//class SortMR : public MapReduce<SortMR, sort_data, intptr_t, uint64_t, common_array_container<sort_word, uint64_t, one_combiner, 1000
//class SortMR : public MapReduce<SortMR, sort_data, int, long long, common_array_container<int, long long, one_combiner, 1000
#endif
#ifdef TBB
    , tbb::scalable_allocator
#endif
> >
{
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
		char *curr_word, *curr_key, curr_ltr;
		int state = IN_KEY;
		
		// Start at the beginning of the input split
		curr_word = s.data;
		curr_key = s.data;

		// Teragen creates 10 byte keys
		int nchar = 0;
	
		// Iterate through the input split and parse on newlines
		for (uint64_t i = 0; i < s.len; i++) {
			curr_ltr = s.data[i];
			switch(state) {
			case IN_KEY: 
				if (nchar == 0)
					curr_word = &s.data[i];	
				if (nchar >= 10) {
					// End of the current word
					s.data[i] = 0;
					state = NOT_IN_WORD;
					curr_key = curr_word;
					nchar = 0;
				}
				else
					nchar++;
				break;
			case IN_VALUE: 
				if (curr_ltr == '\n') {
					// The values end in '\r\n'
					s.data[i] = 0;
					s.data[i-1] = 0;
					state = IN_KEY;

					// Emit the intermediate <k, v> pair
					sort_word word;
					word.key = curr_key;
					word.value = curr_word;
					//printf("Emitting key: %s\n", word.key);
					emit_intermediate(out, word, 0);
					//emit_intermediate(1, 2, 0);
				}
				break;
			default:
			case NOT_IN_WORD:
				if (s.data[i] >= '!' && s.data[i] <= '~') {
					// Start of a new word
					curr_word = &s.data[i];				
					state = IN_VALUE;
				}
				break;
			}
		}
	}

        void reduce(key_type const& key, reduce_iterator const& values, std::vector<keyval>& out) const {
            /* Each key should be unique... O_o */
            value_type val;
            values.next(val);
            
            keyval kv = {key, val};
            out.push_back(kv);
        }

	// split(): split the input file (memory mapped) into words
	int split(sort_data& out)
	{
		// End of the file
		if ((uint64_t) splitter_pos >= data_size)
			return 0;
	
		// If chunk size goes over the end of the file	
		uint64_t end = std::min(splitter_pos + chunk_size, data_size);
		
		// Move end point to next word break
		while(end < data_size && 
			data[end] != '\n')
			end++;
		
		// We are splitting on a newline and not replacing the split character, causing the extraction to fail 
#ifdef NO_MMAP
		data[end] = 0;
#else
		// Can't write beyond the end of an MMAP
		if (end < filesize)
			data[end] = 0;	
#endif

		// Trim the input data
		out.data = data + splitter_pos;
		out.len = end - splitter_pos;
		
		// Set the beginning to the front of the file
		splitter_pos = end;
		
		// Out data is valid
		return 1;
	}

	//bool sort(keyval const& a, keyval const& b) const {
        //        return true;
	////	return a.val < b.val || (a.val == b.val && strcmp(a.key.key, b.key.key) > 0);
	//}

};

int main(int argc, char *argv[]) {
	unsigned int fd, disp_num;
	char *fname, *disp_num_str, *fdata;
	struct stat finfo;
	struct timespec begin, end, total_begin, total_end;
	
	get_time(total_begin);
	get_time(begin);

	if (argv[1] == NULL) {
		printf("USAGE: %s <filename> [ndisplay]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	printf("Sort: Running...\n");

#ifdef TBB
	printf("... using Intel's scalable allocator (TBB)\n");
#endif

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
	printf("Sort: Memory mapping (populate) the file - %lu\n", finfo.st_size);
	CHECK_ERROR( (fdata = (char *) mmap(0, finfo.st_size + 1, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_POPULATE, fd, 0)) == NULL);
	filesize = finfo.st_size;
#else
	printf("Sort: Memory mapping (no populate) the file\n");
	CHECK_ERROR( (fdata = (char *) mmap(0, finfo.st_size + 1, PROT_READ, MAP_PRIVATE, fd, 0) == NULL);
	filesize = finfo.st_size;
#endif
#else
    	uint64_t r = 0;
	// Read into the malloc'd space
	printf("Sort: Mallocing the file\n");
	CHECK_ERROR( (fdata =  (char *) malloc(finfo.st_size + 1)) == NULL);
        r = read(fd, fdata, finfo.st_size);
	//while (r < (uint64_t) finfo.st_size)
	//	r += pread (fd, fdata + r, finfo.st_size, r);
	printf("read: %lu bytes\n", r);
	//CHECK_ERROR( r != (uint64_t) finfo.st_size);
#endif


	get_time(end);
#ifdef TIMING
    print_time("Sort: initialize", begin, end);
#endif
	
	// Start the job
	printf("Sort: Calling MapReduce Scheduler Sort\n");
	get_time(begin);

	// Create the result buffer
	std::vector<SortMR::keyval> result;
	SortMR mapReduce(fdata, finfo.st_size, 1024*1024);
	CHECK_ERROR( mapReduce.run(result) < 0);
	get_time(end);
#ifdef TIMING
    print_time("Sort: library", begin, end);
#endif

	printf("Sort: MapReduce Completed\n");
	get_time(begin);

	// Print the result
	printf("Sort: Results\n");
        //__gnu_parallel::sort(result.begin(), result.end());
	unsigned int dn = std::min(disp_num, (unsigned int)result.size());
	char prev[LINE_MAX];
	CHECK_ERROR( strcpy(prev, "") < 0);
        // What the fuck are you doing? The keyes are unique
        for (uint64_t i = 0; i < dn && i < result.size(); i++) {
            printf("\t%s - %s\n", result[i].key.key, result[i].key.value);
        }
        //uint64_t printed = 0;
	//for (uint64_t i = 0; printed < dn; i++) { 
	//	char curr_key[LINE_MAX], curr_value[LINE_MAX];
	//	if (i >= result.size()) 
	//		break;
	//	CHECK_ERROR( strcpy(curr_key, result[result.size() - 1 - i].key.key) < 0);
	//	CHECK_ERROR( strcpy(curr_value, result[result.size() - 1 - i].key.value) < 0);
	//	if (strcmp(prev, curr_key)) {
	//		CHECK_ERROR( strcpy(prev, curr_key) < 0);
	//		//printf("\t%lu: %s\n", i, curr_word);
	//		printf("\t%s - %s\n", curr_key, curr_value);
	//		printed++;
	//	}

	//}
	printf("Total: %lu\n", result.size());
	get_time(end);
	get_time(total_end);
#ifdef TIMING
    print_time("Sort: finalize", begin, end);
    print_time("Sort: totalinalize", total_begin, total_end);
#endif


#ifndef NO_MMAP
	CHECK_ERROR(munmap(fdata, finfo.st_size + 1) < 0);
#else
	free (fdata);
#endif
	CHECK_ERROR(close(fd) < 0);

	return 0;
}
