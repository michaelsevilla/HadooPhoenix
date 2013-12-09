/** * Edited by msevilla for a scale-up vs. scale-out study.
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
#include "hdfs.h"

#define NCHUNKS_MAX         10000
#define DEFAULT_DISP_NUM    10
#define HASHES              "--------------------------------------------------"

#define DEBUG

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
    job_state *job;
    int id;
    int fileid;
    uint64_t size;              
    char *data;
    uint64_t nread;
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
 * \fn          initialize_chunk
 * \brief       Allocates space for a new chunk and initializes values.
 *
 * \param[in]   chunk       A pointer to the head of the chunk list
 * \param[in]   job         The state/global variables of the job.
 * \param[in]   id          The chunk number
 * \param[in]   fileid      The current file to read from
 * \param[in]   nread       How much we have read so far
 *
 * \return      A pointer to a new chunk.
 *
 * This makes the code a little cleaner
 */
chunk_t *initialize_chunk(chunk_t *chunk, job_state *job, int id, int fileid, uint64_t nread) {
    chunk->job = job;
    chunk->id = id;
    chunk->fileid = fileid;
    chunk->nread = nread;
    chunk->size = 0;
    chunk->data = NULL;

    return chunk;
}


/**
 * \fn          construct_filename
 * \brief       Build the filename from a directory input.
 *
 * \param[in]    job        The state/global variables of the job.
 * \param[in]    filename   Pointer to the space to fill.
 * \param[in]    path       The path to the input directory.
 * \param[in]    file_id    The number from the end of part.
 *
 * Go through the directory and construct the correct filename
 */
void construct_filename (job_state *job, char *filename, int file_id) 
{
        char chunk_id[64] = "";

        strcpy(filename, job->path);
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
 * \param[in]    job     The state/global variables of the job.
 *
 * This is needed so we know when to stop reading/chunking.
 */
uint64_t get_fsize(job_state *job) 
{
    uint64_t input_size = 0;

    for (int i = 0; i < job->total_nfiles; i++) {
        char filename[32];
 
        construct_filename (job, filename, i);
        
        // Fill in the chunk data
        if (job->hdfs != NULL) {
            hdfsFileInfo *finfo = NULL;
            
            CHECK_ERROR( (finfo = hdfsGetPathInfo(job->hdfs, filename)) == NULL);
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
 * \param[in]   job     The state/global variables of the job.
 * \param[in]   chunk   The chunk metadata, which tells where and what to read.
 *
 * \return      0 on success, < 0 on failure
 *
 * Determines how much to read (based on the ingest threshold and number of requested 
 * files. It also allocates space for the chunk data, which is free in the parent function.
 * We only open the file once.
 *
 * TODO: do we want to use the default values for the replication, block size, buffer size (open)
 */
int read_chunk(job_state *job, chunk_t *chunk)
{
    char filename[LINE_MAX] = "";
    uint64_t size = 0;
    uint64_t nfiles = chunk->fileid;

    debug_printf("\t[read_chunk] reading from directory\n");
    for (int i = 0; i < job->ingest_files; i++) {
        char *tmp = NULL;
        off_t fsize = 0;
    
        // Initialized for both types (re-initialied every loop anyways)
        int fd = -1;
        hdfsFile hdfsF = NULL;
    
        if (chunk->fileid >= job->total_nfiles) {
            debug_printf("\t[read_chunk] read in the targeted number of files\n");
            chunk->size = size;
            return 0;
        }
    
        construct_filename(job, filename, nfiles);
        printf("filename = %s\n", filename);
    
        if (job->hdfs != NULL) {
            hdfsFileInfo *finfo = NULL;
            CHECK_ERROR( (hdfsF = hdfsOpenFile(job->hdfs, filename, O_RDONLY, 0, 0, 0)) == NULL);
            CHECK_ERROR( (finfo = hdfsGetPathInfo(job->hdfs, filename)) == NULL);
            fsize = finfo->mSize;
        }
        else {
            struct stat finfo;
            CHECK_ERROR( (fd = open(filename, O_RDONLY)) < 0);
            CHECK_ERROR( fstat(fd, &finfo) < 0);
            fsize = finfo.st_size;
        }
    
        tmp = chunk->data;
        chunk->data = (char *)realloc(chunk->data, size + (uint64_t)fsize);
        if (chunk->data == NULL) {
            free(tmp);   
            fprintf(stderr, "ERROR: can't reallocate memory\n");
            exit(EXIT_FAILURE);
        }
    
        debug_printf("\t[read_chunk] reading file %s (%lu bytes), mallocd %lu bytes\n", filename, fsize, size + fsize);
    
        // Read the chunk (right now, chunk = file size)
        uint64_t r = 0;
        while (r < (uint64_t) fsize) {
            if (job->hdfs != NULL) {
                r += hdfsPread(job->hdfs, hdfsF, r, chunk->data + r + size, fsize);
            }
            else {
                r += pread (fd, chunk->data + r + size, fsize, r);
            }
        }
        chunk->nread = chunk->nread + r;
    
        if (job->hdfs != NULL) {
            CHECK_ERROR( hdfsCloseFile(job->hdfs, hdfsF) < 0);
        }
        else {
            CHECK_ERROR( close(fd) < 0);
        }
    
        size += fsize;
        nfiles++;
        chunk->fileid = nfiles;
    }
    
    chunk->size = size;

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
    read_chunk(chunk->job, chunk);
    debug_printf("\t[pthread_read] thread read %lu bytes\n", chunk->size);

    return (void *) chunk->size;
}

/**
 *  \fn         run_ingest_chunks
 *  \brief      Calls multiple mappers and dumps to one data structure.
 */
int run_ingest_chunks(job_state *job, WordsMR mapReduce, 
    std::vector<WordsMR::keyval> &result, chunk_t **chunks, chunk_t **start)
{
    uint64_t nread = 0;                                         // How much we've read
    int nchunks;
    chunk_t *curr_chunk;                                        
    struct timespec begin, end;

    mapReduce.run_init();

    // Chunk data structures
    job->total_size = get_fsize(job);
    debug_printf("[run_phoenix] total size = %lu\n", job->total_size);
    nchunks = 0;
    start = chunks;

    // Read the first chunk
    *chunks = (chunk_t *)malloc(sizeof(chunk_t));
    curr_chunk = initialize_chunk(*chunks, job, 0, 0, 0);
    read_chunk(job, curr_chunk);
    nread += curr_chunk->size;
    nchunks++;
    *chunks++;

    printf("nread (chunk %i) = %lu\n", nchunks, nread);
    debug_printf("[run_phoenix] curr_chunk stats \n\tid = %d, size = %lu, size = %lu, nread = %lu\n%s\n\n", 
        curr_chunk->id, curr_chunk->size, curr_chunk->size, nread, HASHES);

    // We process one chunk behind the current read 
    while (nread < job->total_size) {
        pthread_t thread1;
        void *ret; 
        chunk_t *prev_chunk = curr_chunk;

        debug_printf("[run_phoenix] creating thread to read the next chunk\n");
        *chunks = (chunk_t *)malloc(sizeof(chunk_t));
        curr_chunk = initialize_chunk(*chunks, job, nchunks, curr_chunk->fileid, curr_chunk->nread);
        CHECK_ERROR (pthread_create (&thread1, NULL, pthread_read, (void *) curr_chunk) < 0);

        debug_printf("[run_phoenix] master thread runs mappers (in parallel w/ read)\n");
        mapReduce.set_data(prev_chunk->data, prev_chunk->size);
        CHECK_ERROR( mapReduce.run_mappers(result) < 0);

        debug_printf("[run_phoenix] waiting for read thread to join\n");
        pthread_join (thread1, &ret);
        debug_printf("[run_phoenix] read thread joined\n");

        // Cleanup and prepare for next iteration
        nread += (uint64_t) ret;
        nchunks++;
        *chunks++;

        printf("nread (chunk %i) = %lu\n", nchunks, nread);
    }

    debug_printf("[run_phoenix] compute on the last chunk (since we read ahead)\n");
    mapReduce.set_data(curr_chunk->data, curr_chunk->size);
    CHECK_ERROR( mapReduce.run(result) < 0);

    get_time (end);
    print_time("Wordcount: mappers", begin, end);

    debug_printf("[run_phoenix] all mappers done, start reducers\n");
    get_time (begin);
    CHECK_ERROR( mapReduce.run_reducers(result) < 0);
    get_time (end);
    print_time("Wordcount: reducers", begin, end);

    return nchunks;
}


/**
 *  \fn         run_job
 *  \brief      The main engine that calls Phoenix init(), map(), and reduce() functions.
 *
 *  \param[in]  job         The parameters to the job.
 *  \param[in]  disp_num    The number of results to display.
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
 *
 * TODO: make the return value of read_chunk meaningful
 */
int run_job(job_state *job, unsigned int disp_num) 
{
    chunk_t **start = NULL;                                     // Start of data chunk list; used for cleanup
    chunk_t **chunks = NULL;
    int nchunks = 0;
    //struct timespec begin, end, total_begin, total_end;

    //get_time (total_begin);
    //get_time (begin);

    printf("Wordcount: Running...\n");
    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    //print_time("Wordcount: initialize libraries", begin, end);


    // Phoenix data structures
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(1024*1024);
    chunks = (chunk_t **)calloc(NCHUNKS_MAX, 1);
    nchunks = run_ingest_chunks(job, mapReduce, result, chunks, start);

    // Print out the results
    printf("Wordcount: MapReduce Completed\n");
    //get_time (begin);
    unsigned int dn = std::min(disp_num, (unsigned int)result.size());
    uint64_t total = 0;
    for(size_t i = 0; i < result.size(); i++)
        total += result[i].val;

    printf("\nWordcount: Results (TOP %d of %lu):\n", dn, result.size());
    for (size_t i = 0; i < dn; i++)
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);
    printf("Total: %lu\n", total);

    // Cleanup
    //for (int i = 0; i < nchunks; i++) {
    //    free(start[i]->data);
    //    free(start[i]);
    //}
    //free(start);
    if (job->hdfs != NULL) {
        CHECK_ERROR( hdfsDisconnect(job->hdfs) < 0);
    }
    //get_time(total_end);
    //get_time(end);
    //print_time("Wordcount: finalize", begin, end);
    //print_time("Wordcount: total", total_begin, total_end);

    return 0;
}

int main(int argc, char *argv[]) 
{
    debug_printf("Trying to figure out callbacks\n");

    unsigned int disp_num;
    char *disp_num_str = NULL;
    int c;

    job_state job = {NULL, "", -1, -1, 1};

    while ((c = getopt(argc, argv, "n:i:qh")) != -1) {
        switch(c) {
        case 'n':
            disp_num_str = optarg;
            break;
        case 'i':
            job.ingest_files = atoi(optarg);
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
            printf("\t-i i\t Ingest i files at a time in parallel with mapeprs\n");
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
            fprintf(stderr, "Wordcount USAGE: %s [options] path\n", argv[0]); 
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

    debug_printf("disp_num = %d; path = %s; nfiles = %lu; ingest_files = %lu\n", 
      disp_num, job.path, job.total_nfiles, job.ingest_files);

    return run_job(&job, disp_num);
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
