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

// a passage from the text. The input data to the Map-Reduce
struct wc_string {
    char* data;
    uint64_t len;
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
    chunk_t **start = NULL;                                     // Start of data chunk list; used for cleanup
    chunk_t **chunks = NULL;
    int nchunks = 0;
    unsigned int dn = 0;
    struct timespec begin, end, total_begin, total_end;

    printf("Wordcount: Running...\n");
    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    get_time(total_begin); get_time(begin);
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(1024*1024);
    chunks = (chunk_t **)calloc(NCHUNKS_MAX, 1);
    get_time(end);
    print_time("initialize", begin,end);

    get_time(begin);
    nchunks = mapReduce.run_ingest_chunks(job, result, chunks, start);
    get_time(end);
    print_time("library", begin, end);

    printf("Wordcount: MapReduce Completed\n");
    get_time(begin);
    dn = std::min(disp_num, (unsigned int)result.size());
    printf("\nWordcount: Results (TOP %d of %lu):\n", dn, result.size());
    for (size_t i = 0; i < dn; i++)
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);
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
        case 'i':
            job.ingest_files = atoi(optarg);
            break;
        case 'q':
            job.hdfs = hdfsConnect("issdm-36", 54310);
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
            printf("\t-b i\t Ingest i bytes at a time in parallel with mappers\n");
            printf("\t-i i\t Ingest i files at a time in parallel with mappers\n");
            printf("\t-q  \t Use an input HDFS directory\n");
            printf("\n");
            printf("Examples:\n");
            printf("\t%s -i 10 /data1/data/randomtextwriter/\n", argv[0]); 
            printf("\t%s -b 1048576 /data1/data/randomtextwriter-input\n", argv[0]); 
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

    debug_printf("disp_num = %d; path = %s; nfiles = %lu; ingest_files = %lu; ingest_bytes = %lu\n", 
      disp_num, job.path, job.total_nfiles, job.ingest_files, job.ingest_bytes);

    run_job(&job, disp_num);
    return 0;
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
