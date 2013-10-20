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
#include "hdfs.h"

//#define INGEST_THRESH       1073741824
//#define INGEST_THRESH       1610612736
#define INGEST_THRESH       4294967296
#define NCHUNKS_MAX         10000
#define DEFAULT_DISP_NUM    10
#define HASHES              "--------------------------------------------------"

//#define DEBUG

// Some ill-advised global variables
int count = 0;
int count_emit = 0;
bool input_dir = false;
bool hdfs_dir = false;
hdfsFS hdfs = NULL;

// We could pass these around in functions but it is easier to for the thread to pull
// these values instead of creating a new struct
char path[LINE_MAX] = "";                           // The path to the file or directory
uint64_t total_size = 0;                            // The size of the whole

uint64_t total_nfiles = 999999999;
int ingest_files = 99999;

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
    char *filename;
    int id;
    int fileid;
    uint64_t size;              
    char *data;
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

/**
 * \fn          construct_filename
 * \brief       Build the filename from a directory input.
 *
 * param[in]    filename    Pointer to the space to fill.
 * param[in]    path        The path to the input directory.
 * param[in]    file_id     The number from the end of part.
 *
 * Go through the directory and construct the correct filename
 */
void construct_filename (char *filename, int file_id) 
{
        char chunk_id[64] = "";

        strcpy(filename, path);
        if (file_id < 10)
            strcat(filename, "/part-0000");
        else if (file_id < 100)
            strcat(filename, "/part-000");
        else if (file_id < 1000)
            strcat(filename, "/part-00");
        else {
            fprintf(stderr, "Error: can't create filename\n");
            exit(EXIT_FAILURE);
        }
        CHECK_ERROR(sprintf(chunk_id, "%d", file_id) < 0);
        strcat(filename, chunk_id);
}


/**
 * \fn          get_fsize
 * \brief       Get the size of the input by opening all files.
 *
 * param[in]    path    The path to the directory.
 *
 * This is needed when we malloc everything.
 */
uint64_t get_fsize(char *path) 
{
    uint64_t input_size = 0;

    for (uint64_t i = 0; i < total_nfiles; i++) {
        char filename[32];

        construct_filename (filename, i);
        
        // Fill in the chunk data
        if (hdfs_dir) {
            hdfsFileInfo *finfo = hdfsGetPathInfo(hdfs, filename);
            input_size += (uint64_t) finfo->mSize;
        }
        else {
            int fd = -1;
            struct stat finfo;
            
            debug_printf("\t[get_fsize] trying to open %s\n", filename);
            CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
            CHECK_ERROR(fstat(fd, &finfo) < 0);
            input_size += finfo.st_size;
            CHECK_ERROR(close(fd) < 0);
        }
    }

    debug_printf("\t[get_fsize] total input size = %lu\n", input_size);
    return input_size;
}


/**
 * \fn          read_chunk
 * \brief       Reads a chunk of data into memory.
 *
 * \param[in]   chunk   The chunk metadata, which tells where and what to read.
 *
 * \return      The amount of data that we have read after the function exits.
 *
 * Determines how much to read (based on the ingest threshold and number of requested 
 * files. It also allocates space for the chunk data, which is free in the parent function.
 * We only open the file once.
 *
 * TODO: We need to handle the case where the threshold is less than the file size
 */
int read_chunk(chunk_t *chunk)
{
    char filename[LINE_MAX] = "";

    if (input_dir) {
        if (hdfs_dir) {
            debug_printf("\t[read_chunk] reading from HDFS directory\n");
        }
        else {
            debug_printf("\t[read_chunk] reading from ext4 directory\n");

            struct stat finfo;
            int fd = -1;
            char data_str[LINE_MAX];
            uint64_t size = 0;
            uint64_t nfiles = chunk->fileid;

            for (int i = 0; size < INGEST_THRESH && i < ingest_files; i++) {
                if ((uint64_t) chunk->fileid >= total_nfiles) {
                    debug_printf("\t[read_chunk] read in the targeted number of files\n");
                    chunk->size = size;
                    return 0;
                }

                // Allocate (more) space for the chunk
                construct_filename(filename, nfiles);
                chunk->filename = filename;
                CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
                CHECK_ERROR(fstat(fd, &finfo) < 0);
                chunk->data = (char *)realloc(chunk->data, size + finfo.st_size);
                printf("\t[read_chunk] reading file %s (%lu bytes)\n", filename, finfo.st_size);

                // Read the chunk (right now, chunk = file size)
                uint64_t r = 0;
                while (r < (uint64_t) finfo.st_size)
                    r += pread (fd, chunk->data + r + size, finfo.st_size, r);
                snprintf(data_str, LINE_MAX, chunk->data);
                debug_printf("\t[read_chunk] file %s (%lu bytes):\n####################\n%s\n####################\n", 
                        filename, finfo.st_size, data_str);
                size += finfo.st_size;
                CHECK_ERROR(close(fd) < 0);

                nfiles++;
                chunk->fileid = nfiles;
            }

            chunk->size = size;
        }
    }
    else {
        debug_printf("\t[read_chunk] reading from a single file\n");
    }

    return 0;
}


/**
 * \fn          pthread_read 
 * \brief       Reads the data in a pthread function.
 *
 * This is pthread function (hence the void * return value and arguments).
 */
void *pthread_read( void *args)
{
    chunk_t *chunk = (chunk_t *) args;

    debug_printf("\t[pthread_read] thread instructed to read\n");
    read_chunk(chunk);
    debug_printf("\t[pthread_read] thread read %lu bytes\n", chunk->size);

    return (void *) chunk->size;
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
 *
 *  Design
 *      read chunk 
 *      while nread < total size
 *          save old data, old size
 *
 *          create pthread
 *          nread += read chunk
 *
 *          run mappers (old data, old size)
 *
 *      run mappers
 */
int run_phoenix(unsigned int disp_num, bool input_dir) {
    char **data = NULL;                                         // One chunk for all mappers/reducers to process
    char **start = NULL;                                        // Start of data chunk list; used for cleanup
    int nchunks = 0;                                            // Track the number of chunks
    uint64_t nread = 0;                                         // How much we've read
    chunk_t curr_chunk = {NULL, 0, 0, NULL};                    // The chunk that gets re-used/abused
    struct timespec begin, end, total_begin, total_end;         // Timing data for debugging

    get_time (total_begin);
    printf("Wordcount: Running...\n");
    get_time (begin);

    // Initialize the libraries
    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    if (input_dir)
        total_size = get_fsize(path);
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(1024*1024);
    mapReduce.run_init();

    // Allocate space for the chunk data and filename
    curr_chunk.filename = (char *) malloc(LINE_MAX * sizeof(char));
    data = (char **)malloc(NCHUNKS_MAX * sizeof(char *));
    start = data;

    get_time (end);
    print_time("Wordcount: initialize libraries", begin, end);
    debug_printf("[run_phoenix] total size = %lu\n", total_size);
    get_time (begin);

    // Read the next chunk 
    curr_chunk.id = 0;
    curr_chunk.data = *data;
    read_chunk (&curr_chunk);
    nread += curr_chunk.size;
    nchunks++;

    printf("nread = %lu\n", nread);
    debug_printf("[run_phoenix] curr_chunk stats \n\tfilename: %s\n\tid = %d, size = %lu, size = %lu, nread = %lu\n%s\n\n", 
        curr_chunk.filename, curr_chunk.id, curr_chunk.size, curr_chunk.size, nread, HASHES);

    // We process one chunk behind the current read 
    while (nread < total_size) {
        pthread_t thread1;
        void *ret; 

        // Save the old data and size
        uint64_t prev_chunk_size = curr_chunk.size;
        char *prev_data = curr_chunk.data;        
      
        debug_printf("[run_phoenix] creating read thread to read\n");

        // Read the chunk with a thread
        curr_chunk.id = nchunks;
        *data++;
        curr_chunk.data = *data;
        CHECK_ERROR (pthread_create (&thread1, NULL, pthread_read, (void *) &curr_chunk) < 0);

        // Execution of the mappers continues in parallel with the read
        debug_printf("[run_phoenix] master thread runs mappers\n");
        mapReduce.set_data(prev_data, prev_chunk_size);
        CHECK_ERROR( mapReduce.run(result) < 0);

        // Cleanup
        debug_printf("[run_phoenix] waiting for read thread to join\n");
        pthread_join (thread1, &ret);
        nread += (uint64_t) ret;
        nchunks++;
        debug_printf("[run_phoenix] read thread joined\n");

        printf("nread  = %lu\n", nread);
    }

    // Compute on the last chunk
    mapReduce.set_data(curr_chunk.data, curr_chunk.size);
    CHECK_ERROR( mapReduce.run(result) < 0);

    get_time (end);
    print_time("Wordcount: mappers", begin, end);

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
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);

    for(size_t i = 0; i < result.size(); i++)
        total += result[i].val;

    printf("Total: %lu\n", total);

    // Cleanup
    data = start;
    for (int i = 0; i < nchunks; i++) 
        free(data[i]);

    free(data);

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
    int c;

    // Parse command line options
    while ((c = getopt(argc, argv, "n:d:i:q:h")) != -1) {
        switch(c) {
        case 'n':
            disp_num_str = optarg;
            break;
        case 'd':
            input_dir = true;
            total_nfiles = atoi(optarg);
            break;
        case 'i':
            ingest_files = atoi(optarg);
            break;
        case 'q':
            input_dir = true;
            hdfs_dir = true;
            total_nfiles = atoi(optarg);
            hdfs = hdfsConnect("localhost", 54310);
            CHECK_ERROR( hdfs == NULL);
            break;
        case 'h':
            printf("Wordcount USAGE: %s [options] path\n\n", argv[0]);
            printf("Flags\n");
            printf("\t-h  \t Print this help menu\n");
            printf("\t-n i\t Display the top i results\n");
            printf("\t-d i\t Use an input dir at path i (instead of a file)\n");
            printf("\t-i i\t Ingest i files at a time in parallel with mapeprs\n");
            printf("\t-q i\t Use an input HDFS dir at path i\n");
            printf("\t-w i\t Use local FS as tier between HFDS dir at path i\n");
            printf("\n");
            printf("Ex: %s -d 10 /data1/data/randomtextwriter/\n", argv[0]); 
            printf("Ex: %s -n 20 /data1/data/randomtextwriter-input\n", argv[0]); 
            exit(EXIT_SUCCESS);
        default: 
            fprintf(stderr, "Wordcount USAGE: %s [options] path\n", argv[0]); 
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }
    strcpy(path, argv[optind]);

    // Get the number of results to display
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);

    debug_printf("disp_num = %d; input_dir = %d; path = %s\n", disp_num, input_dir, path);

    return run_phoenix(disp_num, input_dir);
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
