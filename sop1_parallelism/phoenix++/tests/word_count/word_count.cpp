/**
 * Edited by msevilla for a scale-up vs. scale-out study.
 * edited on dexter
 */


/* Copyright (c) 2007-2011, Stanford University
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Stanford University nor the names of its 
*       contributors may be used to endorse or promote products derived from 
*       this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/ 

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdarg.h>
#include <limits.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"

#define INGEST_THRESH       2147483648
#define NCHUNKS_MAX         10000
#define DEFAULT_DISP_NUM    10

//#define DEBUG

int count = 0;
int count_emit = 0;

int debug_printf(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    return vprintf(fmt, args);
#endif
    return 0;
}

// a passage from the text. The input data to the Map-Reduce
struct wc_string {
    char* data;
    uint64_t len;
};

struct chunk_t {
    int fd;
    int id;                         // The nchunk
    uint64_t size;                  // Where the chunks split
    uint64_t fsize;                 // The size of the actual file
    uint64_t nread;
    char *fdata;
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


// a hash for the word
struct wc_word_hash
{
    // FNV-1a hash for 64 bits
    size_t operator()(wc_word const& key) const
    {
        char* h = key.data;
        uint64_t v = 14695981039346656037ULL;
        while (*h != 0)
            v = (v ^ (size_t)(*(h++))) * 1099511628211ULL;
        return v;
    }
};

#ifdef MUST_USE_FIXED_HASH
class WordsMR : public MapReduceSort<WordsMR, wc_string, wc_word, uint64_t, fixed_hash_container<wc_word, uint64_t, sum_combiner, 32768, wc_word_hash
#else
class WordsMR : public MapReduceSort<WordsMR, wc_string, wc_word, uint64_t, hash_container<wc_word, uint64_t, sum_combiner, wc_word_hash 
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
public:
    explicit WordsMR(uint64_t _chunk_size) :
        chunk_size(_chunk_size), splitter_pos(0) {}

    void set_data(char* _data, uint64_t length)
    {
        data = _data;
        data_size = length;
        splitter_pos = 0;
    }

    void* locate(data_type* str, uint64_t len) const
    {
        return str->data;
    }

    void map(data_type const& s, map_container& out) const
    {
        for (uint64_t i = 0; i < s.len; i++)
        {
            s.data[i] = toupper(s.data[i]);
        }

        uint64_t i = 0;
        while(i < s.len)
        {            
            while(i < s.len && (s.data[i] < 'A' || s.data[i] > 'Z'))
                i++;
            uint64_t start = i;
            while(i < s.len && ((s.data[i] >= 'A' && s.data[i] <= 'Z') || s.data[i] == '\''))
                i++;
            if(i > start)
            {
                s.data[i] = 0;
                wc_word word = { s.data+start };
                emit_intermediate(out, word, 1);
            }
        }
    }

    /** wordcount split()
     *  Memory map the file and divide file on a word border i.e. a space.
     */
    int split(wc_string& out)
    {
        /* End of data reached, return FALSE. */
        if ((uint64_t)splitter_pos >= data_size)
        {
            return 0;
        }

        /* Determine the nominal end point. */
        uint64_t end = std::min(splitter_pos + chunk_size, data_size);

        /* Move end point to next word break */
        while(end < data_size && 
            data[end] != ' ' && data[end] != '\t' &&
            data[end] != '\r' && data[end] != '\n')
            end++;

        /* Set the start of the next data. */
        out.data = data + splitter_pos;
        out.len = end - splitter_pos;
        
        splitter_pos = end;

        return 1;
    }

    bool sort(keyval const& a, keyval const& b) const
    {
        return a.val < b.val || (a.val == b.val && strcmp(a.key.data, b.key.data) > 0);
    }
};

/**
 * Helper functions
 */

void get_chunk(chunk_t *chunk, char* path, bool input_dir) {
    int fd = -1;

    if (input_dir) {
        char fname[LINE_MAX] = "";
        struct stat finfo;

        // Go through the directory until and pull the correct file
        strcpy(fname, path);
        //strcat(fname, "/part-00000");
        strcat(fname, "/test.txt");
        printf("fname: %s\n", fname);
        CHECK_ERROR((fd = open(fname, O_RDONLY)) < 0);
        CHECK_ERROR(fstat(fd, &finfo) < 0);

        chunk->fd = fd;
        chunk->size = (uint64_t) finfo.st_size;
    }
    else {
        uint64_t split, init_split;
        char c = '\0';

        if (chunk->fd < 0) {
            // The file has not been opened yet
            struct stat finfo;

            CHECK_ERROR((fd = open(path, O_RDONLY)) < 0);
            CHECK_ERROR(fstat(fd, &finfo) < 0);
            chunk->fd = fd; 
            chunk->fsize = (uint64_t) finfo.st_size;
        }
    
        // Try a naive split
        init_split = chunk->nread + INGEST_THRESH;

        // Determine if we have reached the end of the file
        if (init_split >= chunk->size)
            chunk->size = (uint64_t) (chunk->fsize - chunk->nread);
        else {
            // Otherwise return the next chunk size
            for (split = init_split; c != ' ' && c != '\n'; split++) 
                CHECK_ERROR(pread(fd, &c, 1, split - 1) < 0);
    
            debug_printf("\t\tchunk size = %lu\n", INGEST_THRESH + (split - init_split) - 1);
            chunk->size = (uint64_t) (INGEST_THRESH + (split - init_split) - 1);
        }
    }
}

// Find the file boundary
void get_file_info(chunk_t *chunk, char *path, uint64_t nchunks) {
    int fd;
    struct stat finfo;
    char fname[LINE_MAX] = "";

    strcpy(fname, path);
    //strcat(fname, "/part-00000");
    strcat(fname, "/test.txt");
    CHECK_ERROR((fd = open(fname, O_RDONLY)) < 0);
    CHECK_ERROR(fstat(fd, &finfo) < 0);

    chunk->fd = fd;
    chunk->size = finfo.st_size;
}

// Find word boundary for chunk
//uint64_t get_split(int fd, uint64_t init_split, uint64_t nread, off_t fsize)
void get_split(chunk_t *chunk, int fd, uint64_t init_split, uint64_t nread)
{
    uint64_t split;
    char c = '\0';
    struct stat finfo;
    uint64_t size;

    CHECK_ERROR(fstat(fd, &finfo) < 0);

    // Determine if we have reached the end of the file
    if (nread + INGEST_THRESH >= (uint64_t) finfo.st_size)
        size = (uint64_t) finfo.st_size - nread;
    else {
        // Otherwise return the next chunk size
        for (split = init_split; c != ' ' && c != '\n'; split++) 
            CHECK_ERROR(pread(fd, &c, 1, split - 1) < 0);

        debug_printf("\t\tchunk size = %lu\n", INGEST_THRESH + (split - init_split) - 1);
        size = INGEST_THRESH + (split - init_split) - 1;
    }

    chunk->size = size;
}

/**
 * \fn          read_chunk
 * \brief       Reads a chunk of data into memory.
 *
 * \param[in]    chunk   The chunk metadata, which tells where and want to read.
 *
 * \return      nread   The amount of data that we have read after the function exits.
 */
uint64_t read_chunk(chunk_t *chunk)
{
    uint64_t r = 0;

    CHECK_ERROR (chunk->fdata == NULL);
    debug_printf("\t\tstarting to read chunk\n");
    while(r < chunk->size) {
        r += pread (chunk->fd, chunk->fdata + r, chunk->size - r, chunk->nread + r);
        debug_printf("\t\tread %lu of %lu bytes\n", r, chunk->size);
    }

    debug_printf("fdata (%lu bytes): \n##########\n%s\n##########\n\n", (uint64_t) strlen(chunk->fdata), chunk->fdata);
    chunk->nread = chunk->nread + r;

    printf("nread = %lu bytes\n", chunk->nread);

    return chunk->nread;
}

void *read_data( void *args)
{
    chunk_t *chunk_args = (chunk_t *) args;

    debug_printf("\tthread instructed to read %lu bytes\n", chunk_args->size);
    chunk_args->nread = read_chunk(chunk_args);

    return (void *) chunk_args->nread;
}


/**
 *  \fn         run_phoenix
 *  \brief      The main engine that calls Phoenix init(), map(), and reduce() functions.
 *
 *  \param[in]  path        The path to the input file or directory. 
 *  \param[in]  disp_num    The number of results to display.
 *  \param[in]  input_dir   A flag indicating the input is either a file or directory.
 *
 *  \return     A 0 on success and a -1 on a failure.
 *
 *  This is different than the origianl Phoenix word count application because we allow
 *  the user to control the MapReduce functions. This allows us to call map() multiple 
 *  time so that we can read and map() data in chunks. This flexibility gives us the
 *  ability to run the read as a separate thread so that we don't remain idle while 
 *  waiting for the disk. 
 *
 *  Notes
 *      - removed mmap because we can't mmap part of a file
 */
int run_phoenix(char *path, unsigned int disp_num, bool input_dir) {
    char **fdata = (char **) malloc(NCHUNKS_MAX * sizeof(char *)); // One chunk for all mappers/reducers to process
    char **start;                                               // Pointer to start of the array of pointers; used for cleanup
    //char *prev_fdata;                                           // Chunk of last round (so we can stagger reads/map tasks)
    char *fname;                                                
    struct timespec begin, end, total_begin, total_end;
    int nchunks = 0;
    chunk_t curr_chunk = {-1, 0, 0, 0, NULL};

    get_time (total_begin);

    // Initialize the library
    printf("Wordcount: Running...\n");
    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    get_time (begin);
    fname = path;
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(1024*1024);
    mapReduce.run_init();
    start = fdata;
    get_time (end);
    print_time("Wordcount: initialize libraries", begin, end);
    get_time (begin);

    // Read the first chunk
    get_chunk(&curr_chunk, path, input_dir);
    *fdata = (char *)malloc(curr_chunk.size + 1);
    curr_chunk.id = nchunks;
    curr_chunk.fdata = *fdata;
    read_chunk (&curr_chunk);
    nchunks++;

    // We process one chunk behind the current read 
//    while( nread < (uint64_t) finfo.st_size) {
//        pthread_t thread1;
//        chunk_t chunk_args = {fd, 0, nread, NULL};
//        uint64_t prev_chunk_size = chunk_size;
//        void *ret;
//       
//        // Create a thread to read data into memory
//        chunk_size = find_split(fd, nread + INGEST_THRESH, nread, finfo.st_size);
//        debug_printf("\tcreating read thread to read %lu bytes\n", chunk_size);
//
//        // Read the next chunk
//        prev_fdata = *fdata;
//        *fdata++;
//        *fdata = (char *) malloc(chunk_size + 1);
//        debug_printf("\tthread mallocd chunk: %lu bytes\n", chunk_size);
//
//        // Spawn a thread to read the data in parallel
//        chunk_args.chunk_size = chunk_size;
//        chunk_args.fdata = fdata;
//        CHECK_ERROR (pthread_create (&thread1, NULL, read_data, (void *) &chunk_args) < 0);
//
//        // Execution of the mappers continues in parallel with the read
//        debug_printf("\tmaster thread runs mappers\n");
//        mapReduce.set_data(prev_fdata, prev_chunk_size);
//        CHECK_ERROR( mapReduce.run(result) < 0);
//
//        // Cleanup
//        debug_printf("\twaiting for other thread to join\n");
//        pthread_join (thread1, &ret);
//        nread = (uint64_t ) ret;
//        nchunks++;
//        debug_printf("\tread thread returned\n");
//
//        printf("nread = %lu bytes\n", nread);
//    }

    // Compute on the last chunk
    mapReduce.set_data(*fdata, curr_chunk.size);
    CHECK_ERROR( mapReduce.run(result) < 0);
    get_time (end);
    print_time("Wordcount: all mappers", begin, end);

    // All mappers are complete; run the reducers
    get_time (begin);
    CHECK_ERROR( mapReduce.run_reducers(result) < 0);
    get_time (end);
    print_time("Wordcount: reducers", begin, end);
    printf("Wordcount: MapReduce Completed\n");

    // Print out the results
    get_time (begin);
    unsigned int dn = std::min(disp_num, (unsigned int)result.size());
    printf("\nWordcount: Results (TOP %d of %lu):\n", dn, result.size());
    uint64_t total = 0;
    for (size_t i = 0; i < dn; i++)
    {
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);
    }

    for(size_t i = 0; i < result.size(); i++)
    {
        total += result[i].val;
    }

    printf("Total: %lu\n", total);

    // Clealnup
    fdata = start;
    for (int i = 0; i < nchunks; i++) 
        free(fdata[i]);

    free(fdata);
    CHECK_ERROR(close(curr_chunk.fd) < 0);

    // Get final timings
    get_time(total_end);
    get_time(end);
    print_time("Wordcount: finalize", begin, end);
    print_time("Wordcount: total", total_begin, total_end);

    return 0;
}

int main(int argc, char *argv[]) 
{
    unsigned int disp_num;
    char *disp_num_str = NULL;
    char *path = NULL;
    int c;
    bool input_dir = false;

    // Parse command line options
    while ((c = getopt(argc, argv, "n:d")) != -1) {
        switch(c) {
        case 'n':
            disp_num_str = optarg;
            break;
        case 'd':
            input_dir = true;
            break;
        default: 
            fprintf(stderr, "Wordcount USAGE: %s [options] path\n", argv[0]); 
            fprintf(stderr, "\t Ex: %s -d /data1/data/randomtextwriter/\n", argv[0]); 
            fprintf(stderr, "\t Ex: %s -n 20 /data1/data/randomtextwriter-input\n", argv[0]); 
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }
    path = argv[optind];

    // Get the number of results to display
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);

    debug_printf("disp_num = %d; input_dir = %d; path = %s\n", disp_num, input_dir, path);

    return run_phoenix(path, disp_num, input_dir);
}


// vim: ts=8 sw=4 sts=4 smarttab smartindent
