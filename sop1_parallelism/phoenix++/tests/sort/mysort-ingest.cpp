
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
unsigned long filesize = 0;

enum {
    IN_KEY,
    IN_VALUE,
    NOT_IN_WORD
};
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
class SortMR : public MapReduceSort<SortMR, sort_data, sort_word, uint64_t, hash_container<sort_word, uint64_t, sum_combiner, sort_word_hash >>
{
    char* data;
    uint64_t data_size;
    uint64_t chunk_size;
    uint64_t splitter_pos;
    char curr_word[LINE_MAX];
public:
    explicit SortMR(uint64_t _chunk_size) :
    	chunk_size(_chunk_size), splitter_pos(0) {}

    void set_data(char* _data, uint64_t length) {
        data = _data;
        data_size = length;
        splitter_pos = 0;
    }
            
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
                    s.data[i - 1] = 0;
    	    	    state = IN_KEY;
    
    	    	    // Emit the intermediate <k, v> pair
    	    	    sort_word word;
    	    	    word.key = curr_key;
    	    	    word.value = curr_word;
    	    	    //debug_printf("[mysort-ingest] Emitting key: %s\n", word.key);
    	    	    emit_intermediate(out, word, 0);
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
    	data[end] = 0;
    
    	// Trim the input data
    	out.data = data + splitter_pos;
    	out.len = end - splitter_pos;
    	
    	// Set the beginning to the front of the file
    	splitter_pos = end;
    	
    	// Out data is valid
    	return 1;
    }
};


/**
 *  \fn         run_job
 *  \brief      The main engine that calls Phoenix init(), map(), and reduce() functions.
 *
 *  \param[in]  job         The parameters to the job.
 *  \param[in]  disp_num    The number of results to display.
 *
 *  This is different than the origianl Phoenix word count application because we allow
 *  the user to control the MapReduce functions. This allows us to call map() multiple 
 *  time so that we can read and map() data in chunks. This flexibility gives us the
 *  ability to run the read as a separate thread so that we don't remain idle while 
 *  waiting for the disk. 
 */
void run_job(job_state *job, unsigned int disp_num)
{
    chunk_t **start = NULL;
    chunk_t **chunks = NULL;
    int nchunks = 0;
    unsigned int dn = 0;
    struct timespec begin, end, total_begin, total_end;
    
    printf("Sort: Running...\n");
    printf("Sort: Calling MapReduce Scheduler Sort\n");
    get_time(total_begin); get_time(begin);
    std::vector<SortMR::keyval> result;
    SortMR mapReduce(1024*1024);
    chunks = (chunk_t **)calloc(NCHUNKS_MAX, 1);
    get_time(end);
    print_time("initialize", begin, end);

    get_time(begin);
    nchunks = mapReduce.run_ingest_chunks(job, result, chunks, start);
    get_time(end);
    print_time("library", begin, end);

    printf("Sort: MapReduce Completed\n");
    get_time(begin);
    printf("Sort: Results\n");
    dn = std::min(disp_num, (unsigned int)result.size());
    char prev[LINE_MAX];
    CHECK_ERROR( strcpy(prev, "") < 0);
    for (uint64_t i = 0; i < dn && i < result.size(); i++) {
        if (strlen(result[i].key.key) == 0)
            dn++;
        else
            printf("\t%s - %s\n", result[i].key.key, result[i].key.value);
    }
    
    printf("Total: %lu\n", result.size());

    for (int i = 0; i < nchunks; i++) {
        free(chunks[i]->data);
        free(chunks[i]);
    }
    free(chunks);
    if (job->hdfs != NULL) {
        CHECK_ERROR( hdfsDisconnect(job->hdfs) < 0);
    }

    get_time(end); get_time(total_end);
    print_time("finalize", begin, end);
    print_time("total", total_begin, total_end);

}

int main(int argc, char *argv[]) 
{
    unsigned int disp_num;
    char *disp_num_str = NULL;
    int c;

    job_state job = {NULL, "", -1, -1, 1};

    while ((c = getopt(argc, argv, "b:n:i:qh")) != -1) {
        switch(c) {
        case 'b':
            job.ingest_bytes = atoi(optarg);
            break;
        case 'n':
            disp_num_str = optarg;
            break;
        case 'q':
            job.hdfs = hdfsConnect("localhost", 54310);
            CHECK_ERROR( job.hdfs == NULL);
            break;
        case 'h':
            printf("Wordcount USAGE: %s [options] <NFILES> <PATH>\n\n", argv[0]);
            printf("Description\n");
            printf("\tNFILES   The number of total files to process\n");
            printf("\tPATH     Path to the directory to process part-* files\n");
            printf("Flags\n");
            printf("\t-h  \t Print this help menu\n");
            printf("\t-n i\t Display the top i results\n");
            printf("\t-b i\t Ingest i bytes at a time in parallel with mapeprs\n");
            printf("\t-q i\t Use an input HDFS directory at path i\n");
            printf("\n");
            printf("Examples:\n");
            printf("\t%s -d 10 /data1/data/randomtextwriter/\n", argv[0]); 
            printf("\t%s -n 20 /data1/data/randomtextwriter-input\n", argv[0]); 
            printf("\n");
            printf("*Note: you can no longer specify one file - you must give a\n");
            printf("directory from which to read files part-*.\n");
            exit(EXIT_SUCCESS);
        default: 
            fprintf(stderr, "Sort USAGE: %s [options] path\n", argv[0]); 
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc - 1) {
        fprintf(stderr, "Expected argument(s) after options\n");
        exit(EXIT_FAILURE);
    }
    job.total_nfiles = atoi(argv[optind]);
    strcpy(job.path, argv[optind + 1]);

    // Get the number of results to display
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);

    debug_printf("disp_num = %d; path = %s; nfiles = %lu; ingest_files = %lu; ingest_bytes = %lu\n", 
      disp_num, job.path, job.total_nfiles, job.ingest_files, job.ingest_bytes);

    run_job(&job, disp_num);
   
    return 0;
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
