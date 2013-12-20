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

#define NO_MMAP
//#define TBB
unsigned long filesize = 0;
int total_nfiles = 0;
bool hdfs_dir = false;
hdfsFS hdfs = NULL;

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
};

int main(int argc, char *argv[]) 
{
    char * fdata = NULL;
    unsigned int disp_num;
    struct stat finfo;
    const char * fname, * disp_num_str = "10";
    struct timespec begin, end, total_begin, total_end;
    uint64_t input_size = 0;
    int c;
    uint64_t r = 0;
    char filename[LINE_MAX];
    int nchunks = 0;
    char nchunks_str[32];
    uint64_t nread = 0;
    int fd = -1;

    get_time(total_begin); get_time(begin);
    while ((c = getopt(argc, argv, "n:dqh")) != -1) {
        switch(c) {
        case 'n':
            disp_num_str = optarg;
            break;
        case 'q':
            hdfs_dir = true;
            hdfs = hdfsConnect("localhost", 54310);
            CHECK_ERROR( hdfs == NULL);
            break;
         case 'h':
             printf("Sort USAGE: %s [options] path\n\n", argv[0]);
             printf("Flags\n");
             printf("\t-h  \t Print this help menu\n");
             printf("\t-n i\t Display the top i results\n");
             printf("\t-d i\t Use an input dir at path i (instead of a file)\n");
             printf("\t-q  \t Use an input HDFS directory\n");
             printf("\n");
             printf("Ex: %s -d 10 /data1/data/randomtextwriter/\n", argv[0]);
             printf("Ex: %s -n 20 /data1/data/randomtextwriter-input\n", argv[0]);
             exit(EXIT_SUCCESS);
         default:
             fprintf(stderr, "Sort USAGE: %s [options] path\n", argv[0]);
             exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc - 1) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }
    total_nfiles = atoi(argv[optind]);
    fname = argv[optind + 1];

    printf("Sort: Running...\n");
    // Get the size of the input
    while (nchunks < total_nfiles) {
        strcpy(filename, fname);
        if (nchunks < 10)
            strcat(filename, "/part-0000");
        else if (nchunks < 100)
            strcat(filename, "/part-000");
        else if (nchunks < 1000)
            strcat(filename, "/part-00");
        else {
            fprintf(stderr, "Error: too many files in the directory\n");
            exit(EXIT_FAILURE);
        }
        sprintf(nchunks_str, "%d", nchunks);
        strcat(filename, nchunks_str);

        if (hdfs_dir) {
            hdfsFileInfo *finfo = hdfsGetPathInfo(hdfs, filename);
            CHECK_ERROR( finfo == NULL);
            input_size += (uint64_t) finfo->mSize;
        }
        else {
            CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
            CHECK_ERROR(fstat(fd, &finfo) < 0);
            input_size += finfo.st_size;
            CHECK_ERROR(close(fd) < 0);
        }
        nchunks++;
    }

    fdata = (char *)malloc(input_size + 1);
    CHECK_ERROR (fdata == NULL);

    // Read in the input
    nchunks = 0;
    while (nchunks < total_nfiles) {
        strcpy(filename, fname);
        if (nchunks < 10)
            strcat(filename, "/part-0000");
        else if (nchunks < 100)
            strcat(filename, "/part-000");
        else if (nchunks < 1000)
            strcat(filename, "/part-00");
        else {
            fprintf(stderr, "Error: too many files in the directory\n");
            exit(EXIT_FAILURE);
        }
        sprintf(nchunks_str, "%d", nchunks);
        strcat(filename, nchunks_str);

        r = 0;
        if (hdfs_dir) {
            hdfsFile f = hdfsOpenFile(hdfs, filename, O_RDONLY, 0, 0, 0);
            hdfsFileInfo *finfo = hdfsGetPathInfo(hdfs, filename);
            CHECK_ERROR( f == NULL);
            CHECK_ERROR( finfo == NULL);

            while (r < (uint64_t) finfo->mSize) 
                r += hdfsPread(hdfs, f, r, fdata + r + nread, finfo->mSize - r);

            CHECK_ERROR(hdfsCloseFile(hdfs, f) < 0);
        }
        else {
            CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
            CHECK_ERROR(fstat(fd, &finfo) < 0);
            while (r < (uint64_t) finfo.st_size) 
                r += pread (fd, fdata + r + nread, finfo.st_size, r);
            CHECK_ERROR(close(fd) < 0);
        }
        
        nread += r;
        nchunks++;

        printf("filename: %s\n", filename);
        printf("nread: %lu\n", nread);
    }
   
    std::vector<SortMR::keyval> result;    
    SortMR mapReduce(fdata, input_size, 1024*1024);
    get_time (end);
    print_time("initialize", begin, end);

    printf("Sort: Calling MapReduce Scheduler Sort\n");
    get_time (begin);
    CHECK_ERROR( mapReduce.run(result) < 0);
    get_time (end);
    print_time("library", begin, end);

    printf("Sort: MapReduce Completed\n");
    get_time(begin);
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);
    unsigned int dn = std::min(disp_num, (unsigned int)result.size());
    printf("\nSort: Results (TOP %d of %lu):\n", dn, result.size());
    for (uint64_t i = 0; i < dn && i < result.size(); i++) {
       if (strlen(result[i].key.key) == 0)
            dn++;
       else
            printf("\t%s - %s\n", result[i].key.key, result[i].key.value);
    }
    printf("Total: %lu\n", result.size());
    free (fdata);
    get_time(end); get_time(total_end);
    print_time("finalize", begin, end);
    print_time("total", total_begin, total_end);

    return 0;
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
