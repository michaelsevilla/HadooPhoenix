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
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef TBB
#include "tbb/scalable_allocator.h"
#endif

#include "map_reduce.h"
#include "hdfs.h"


#define DEFAULT_DISP_NUM 10

int total_nfiles = 0;
bool hdfs_dir = false;
hdfsFS hdfs = NULL;


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
    explicit WordsMR(char* _data, uint64_t length, uint64_t _chunk_size) :
        data(_data), data_size(length), chunk_size(_chunk_size), 
            splitter_pos(0) {}

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

        /* Return true since the out data is valid. */
        return 1;
    }

    bool sort(keyval const& a, keyval const& b) const
    {
        return a.val < b.val || (a.val == b.val && strcmp(a.key.data, b.key.data) > 0);
    }
};

int main(int argc, char *argv[]) 
{
    const char * fname, * disp_num_str = "10";
    char filename[LINE_MAX], nchunks_str[32] = "";
    char * fdata = NULL, c;
    int nchunks = 0, fd = -1;
    unsigned int disp_num;
    uint64_t input_size = 0, r = 0, nread = 0;
    struct stat finfo;
    struct timespec begin, end, total_begin, total_end;

    get_time(total_begin); get_time(begin);
    while ((c = getopt(argc, argv, "n:dqh")) != -1) {
        switch(c) {
        case 'n':
            disp_num_str = optarg;
            break;
        case 'q':
            hdfs_dir = true;
            //hdfs = hdfsConnect("localhost", 54310);
            hdfs = hdfsConnect("issdm-36", 54310);
            CHECK_ERROR( hdfs == NULL);
            break;
         case 'h':
             printf("Wordcount USAGE: %s [options] path\n\n", argv[0]);
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
             fprintf(stderr, "Wordcount USAGE: %s [options] path\n", argv[0]);
             exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc - 1) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }
    total_nfiles = atoi(argv[optind]);
    fname = argv[optind + 1];

    printf("Wordcount: Running...\n");
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
    std::vector<WordsMR::keyval> result;    
    WordsMR mapReduce(fdata, input_size, 1024*1024);
    get_time (end);
    print_time("initialize", begin, end);

    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    get_time (begin);
    CHECK_ERROR( mapReduce.run(result) < 0);
    get_time(end);
    print_time("library", begin, end);

    printf("Wordcount: MapReduce Completed\n");
    get_time(begin);
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ? 
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);
    unsigned int dn = std::min(disp_num, (unsigned int)result.size());
    printf("\nWordcount: Results (TOP %d of %lu):\n", dn, result.size());
    for (size_t i = 0; i < dn; i++) {
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);
    }
    printf("Total: %lu\n", result.size());
    free (fdata);
    get_time(end); get_time(total_end);
    print_time("finalize", begin, end);
    print_time("total", total_begin, total_end);

    return 0;
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent
