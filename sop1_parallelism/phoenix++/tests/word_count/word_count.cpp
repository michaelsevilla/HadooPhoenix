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

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"

//#define INGEST_THRESH       100
//#define INGEST_THRESH       1073741824
#define INGEST_THRESH       2147483648
//#define INGEST_THRESH       3221225472
//#define INGEST_THRESH       4294967296
//#define INGEST_THRESH       5368709120
//#define INGEST_THRESH       10737418240
//#define INGEST_THRESH       21474836480
#define NCHUNKS_MAX         10000
#define DEFAULT_DISP_NUM    10
//#define DEBUG
#define NO_MMAP

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
    uint64_t chunk_size;
    uint64_t nread;
    char **fdata;
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

// Find word boundary for chunk
uint64_t find_split(int fd, uint64_t init_split, uint64_t nread, off_t fsize)
{
    uint64_t split;
    char c = '\0';

    // Determine if we have reached the end of the file
    if (nread + INGEST_THRESH >= (uint64_t) fsize)
        return (uint64_t) fsize - nread;

    // Otherwise return the next chunk size
    for (split = init_split; c != ' ' && c != '\n'; split++) 
        CHECK_ERROR(pread(fd, &c, 1, split - 1) < 0);

    debug_printf("\t\tchunk size = %lu\n", INGEST_THRESH + (split - init_split) - 1);
    return INGEST_THRESH + (split - init_split) - 1;
}

/**
 * \fn          read_chunk
 * \brief       Reads a chunk of data into memory.
 *
 * param[in]    fd      The file descriptor to read from.
 * param[in]    fdata   The pointer to malloc data to.
 * param[in]    nread   The amount of data that we have already read.
 *
 * \return      nread   The amount of data that we have read after the function exits.
 */
uint64_t read_chunk(int fd, char *fdata, uint64_t chunk_size, uint64_t nread)
{
    uint64_t r = 0;

    CHECK_ERROR (fdata == NULL);
    debug_printf("\t\tstarting to read chunk\n");
    while(r < (uint64_t) chunk_size) {
        r += pread (fd, fdata + r, chunk_size - r, nread + r);
        debug_printf("\t\tread %lu of %lu bytes\n", r, chunk_size);
    }
    debug_printf("fdata (%lu bytes): \n##########\n%s\n##########\n\n", (uint64_t) strlen(fdata), fdata);

    return nread + r;
}

void *read_data( void *args)
{
    chunk_t *chunk_args = (chunk_t *) args;
    int fd =                chunk_args->fd;
    uint64_t nread =        chunk_args->nread;
    uint64_t chunk_size =   chunk_args->chunk_size;
    char **fdata =          chunk_args->fdata;

    debug_printf("\tthread instructed to read %lu bytes\n", chunk_size);
    nread = read_chunk(fd, *fdata, chunk_size, nread);

    return (void *) nread;
}

/**
 * Sample application that requires explicit calling of MapReduce phases
 *      init()
 *      map()
 *      reduce()
 *  ... which allows multiple map rounds
 *
 *  Notes
 *      - removed mmap because we can't mmap part of a file
 */
int main(int argc, char *argv[]) 
{
    int fd;
    char **fdata = (char **) malloc(NCHUNKS_MAX * sizeof(char *));       // One chunk for all mappers/reducers to process
    char **start;
    unsigned int disp_num;
    struct stat finfo;
    char * fname, * disp_num_str;
    uint64_t chunk_size = 0;                                    // Where the chunks split
    uint64_t nread = 0;                                         // How much of the file we have read thus far
    char *prev_fdata;                                           // Chunk of last round
    struct timespec begin, end, total_begin, total_end;
    int nchunks = 0;

    get_time (total_begin);

    // Make sure a filename is specified
    if (argv[1] == NULL)
    {
        printf("Wordcount USAGE: %s <filename> [Top # of results to display]\n", argv[0]);
        exit(1);
    }

    printf("Wordcount: Running...\n");

    // Get the input file
    fname = argv[1];
    disp_num_str = argv[2];
    CHECK_ERROR((fd = open(fname, O_RDONLY)) < 0);
    CHECK_ERROR(fstat(fd, &finfo) < 0);

    // Get the number of results to display
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);

    // Initialize the library
    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    get_time (begin);
    start = fdata;
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(1024*1024);
    mapReduce.run_init();
    get_time (end);
    print_time("Wordcount: initialize libraries", begin, end);

    // Read the first chunk
    get_time (begin);
    chunk_size = find_split(fd, INGEST_THRESH, nread, finfo.st_size);
    printf("Mallocing the data\n");
    *fdata = (char *)malloc(chunk_size + 1);
    nread = read_chunk (fd, *fdata, chunk_size, nread);
    printf("nread = %lu bytes\n", (uint64_t) nread);
    nchunks++;

    // We to process one chunk behind the current read 
    while( nread < (uint64_t) finfo.st_size) {
        pthread_t thread1;
        chunk_t chunk_args = {fd, 0, nread, NULL};
        uint64_t prev_chunk_size = chunk_size;
        void *ret;
       
        // Create a thread to read data into memory
        chunk_size = find_split(fd, nread + INGEST_THRESH, nread, finfo.st_size);
        debug_printf("\tcreating read thread to read %lu bytes\n", chunk_size);

        // Read the next chunk
        prev_fdata = *fdata;
        *fdata++;
        *fdata = (char *) malloc(chunk_size + 1);
        debug_printf("\tthread mallocd chunk: %lu bytes\n", chunk_size);

        // Spawn a thread to read the data in parallel
        chunk_args.chunk_size = chunk_size;
        chunk_args.fdata = fdata;
        CHECK_ERROR (pthread_create (&thread1, NULL, read_data, (void *) &chunk_args) < 0);

        // Execution of the mappers continues in parallel with the read
        debug_printf("\tmaster thread runs mappers\n");
        mapReduce.set_data(prev_fdata, prev_chunk_size);
        CHECK_ERROR( mapReduce.run(result) < 0);

        // Cleanup
        debug_printf("\twaiting for other thread to join\n");
        pthread_join (thread1, &ret);
        nread = (uint64_t ) ret;
        nchunks++;
        debug_printf("\tread thread returned\n");

        printf("nread = %lu bytes\n", nread);
    }

    // Compute on the last chunk
    mapReduce.set_data(*fdata, chunk_size);
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
    CHECK_ERROR(close(fd) < 0);
    get_time(total_end);
    get_time(end);
    print_time("Wordcount: finalize", begin, end);
    print_time("Wordcount: total", total_begin, total_end);

    return 0;
}


// vim: ts=8 sw=4 sts=4 smarttab smartindent
