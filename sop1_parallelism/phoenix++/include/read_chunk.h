#include "hdfs.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

#define NCHUNKS_MAX         10000
#define DEFAULT_DISP_NUM    10
#define HASHES              "--------------------------------------------------"

// encapsulate the global variables so we can pass it around
struct job_state {
    hdfsFS hdfs;                // HDFS instance to read to
    char path[LINE_MAX];        // The path to the file or directory
    uint64_t total_size;        // The size of the whole
    int total_nfiles;           // The total number of files to read
    int ingest_files;           // The number of files to ingest at one time
    unsigned int ingest_lines;  // The number of lines to ingest at one time
};

// a single ingest chunk
struct chunk_t {
    job_state *job;
    int id;
    int fileid;
    uint64_t size;              
    char *data;
    uint64_t nread;
};


int debug_printf(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    return vprintf(fmt, args);
#endif
    return 0;
}

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
            job->ingest_nlines = (unsigned int) finfo.st_size / 100.0; 
            CHECK_ERROR(close(fd) < 0);
        }
    }

    debug_printf("\t[get_fsize] total input size = %lu\n", input_size);
    return input_size;
}

int read_chunk_bytes(job_state *job, chunk_t *chunk)
{
    char filename[LINE_MAX] = "";
    int fd = -1;
    off_t fsize = -1;
    unsigned int nlines = 0;
    unsigned int total_nlines = 0;
    
    debug_printf("\t[read_chunk_bytes] reading from a directory (chunk->fileid = %lu)\n", chunk->fileid);
    construct_filename(job, filename, chunk->fileid);
    CHECK_ERROR( (fd = open(filename, O_RDONLY)) < 0);

    while (nlines < job->ingest_lines && nlines < job->ingest_nlines) {
        
        nlines++;
    }

    // Find split
    char c = 'a'; 
    off_t split = (off_t) job->ingest_bytes + chunk->nread;
    if (split > fsize) {
        split = fsize + 1;
        chunk->fileid++;
    }
    else {
        CHECK_ERROR( lseek(fd, split, SEEK_SET) < split);
        //while ( c != ' ' && c != '\n' && c != '\r' && c != '\0') {
        while ( c != '\n' ) {
            CHECK_ERROR( read(fd, &c, 1) < 1);
            debug_printf("\t[read_chunk_bytes] c = %c (split = %lu)\n", c, split); 
            split++;
        }
    }
    chunk->size = (uint64_t) split - chunk->nread - 1;
    chunk->data = (char *)malloc(chunk->size + 1);
    debug_printf("\t[read_chunk_bytes] reading file %s (%lu bytes), mallocd %lu bytes\n", filename, fsize, chunk->size);

    uint64_t r = 0;
    debug_printf("\t[read_chunk_bytes] chunk->nread = %lu\n", chunk->nread);
    while (r < (uint64_t) chunk->size) {
        r += pread(fd, chunk->data + r, chunk->size, chunk->nread + r);
    }
    chunk->nread = chunk->nread + r;
    debug_printf("\t[read_chunk_bytes] read chunk: (nread = %lu)\n---\n%s\n---\n", chunk->nread, chunk->data);
 
    CHECK_ERROR( close(fd) < 0);
    return 0;
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
        chunk->data = (char *)realloc(chunk->data, size + fsize + 1);
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
        //chunk->nread = chunk->nread + r;
        chunk->nread = 0;
    
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
    if (chunk->job->ingest_bytes > 0) {
        read_chunk_bytes(chunk->job, chunk);
    }
    else {
        read_chunk(chunk->job, chunk);
    }
    debug_printf("\t[pthread_read] thread read %lu bytes\n", chunk->size);

    return (void *) chunk->size;
}

