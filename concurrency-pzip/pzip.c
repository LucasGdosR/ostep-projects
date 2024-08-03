#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHUNK_SIZE 1024
typedef unsigned int uint;
typedef struct  {
    char *start; // Points to the memory mapped chunk of the file
    char *buf;   // Compressed data
    uint length, buf_length;
} Chunk;

void *zip(void *chunks);
_Atomic uint chunks_shared_index;
uint total_chunks = 0;

int main(int argc, char const *argv[])
{
    if (argc == 1) {
        printf("wzip: file1 [file2 ...]\n");
        exit(1);
    }

    // Aux data structures
    uint chunks_per_file[argc - 1];
    uint last_chunk_sizes[argc - 1];
    char *mmaps[argc - 1];
    size_t file_sizes[argc - 1];
    
    // Get file metadata and mmap
    for (uint i = 0; i < (argc - 1); i++) {
        // File system stuff
        int fd = open(argv[i + 1], O_RDONLY, 0);
        if (fd == -1) {
            perror("Error opening file");
            exit(1);
        }
        struct stat stat;
        if (fstat(fd, &stat) == -1) {
            perror("Error getting file size");
            close(fd);
            exit(1);
        }
        size_t length = stat.st_size;
        file_sizes[i] = length;

        mmaps[i] = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mmaps[i] == MAP_FAILED) {
            perror("Error mapping file");
            close(fd);
            exit(1);
        } close(fd);
        
        // Chunk basic math
        uint chunks_this_file = (length + CHUNK_SIZE - 1) / CHUNK_SIZE;
        chunks_per_file[i] = chunks_this_file;
        last_chunk_sizes[i] = length % CHUNK_SIZE;
        total_chunks += chunks_this_file;
    }

    // Build the main shared array of structs
    Chunk chunks[total_chunks];

    // Allocate a single large block for all chunks
    void *memory_block = malloc(total_chunks * (CHUNK_SIZE * (sizeof(int) + 1)));
    if (memory_block == NULL) {
        perror("Malloc error");
        exit(1);
    }

    // Init chunks iterating over files
    for (uint i = 0, j = 0; j < argc - 1; j++) {

        // Iterate over chunks in this file
        uint chunks_this_file_except_last = chunks_per_file[j] - 1;
        for (uint k = 0; k < chunks_this_file_except_last; k++, i++) {
            Chunk *chunk = &chunks[i];
            chunk->length = CHUNK_SIZE;
            chunk->start = mmaps[j] + k * CHUNK_SIZE;
            chunk->buf = memory_block + i * (sizeof(int) + 1) * CHUNK_SIZE;
        }
        // Deal with edge case
        Chunk *last_chunk = &chunks[i];
        last_chunk->length = last_chunk_sizes[j];
        last_chunk->start = mmaps[j] + (chunks_per_file[j] - 1) * CHUNK_SIZE;
        last_chunk->buf = memory_block + (sizeof(int) + 1) * CHUNK_SIZE * i++;
    }
    
    // Init atomic index
    chunks_shared_index = 0;
    
    // Thread routine
    uint cores = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t tid[cores];
    for (uint i = 0; i < cores; i++) {
        if (pthread_create(&tid[i], NULL, zip, (void *)chunks) != 0) {
            perror("Error spawning thread");
            free(memory_block);
            exit(1);
        }
    }
    for (uint i = 0; i < cores; i++) {
        if (pthread_join(tid[i], NULL) != 0) {
            perror("Error joining thread");
            free(memory_block);
            exit(1);
        }
    }

    // We're done with the files.
    for (uint i = 0; i < (argc - 1); i++) {
        if (munmap(mmaps[i], file_sizes[i]) == -1) {
            perror("Error unmapping file");
            exit(1);
        }
    }
    
    // Deal with chunks that should be compressed together
    // Two pointers: left and right
    for (uint l = 0, r = 1; r < total_chunks; r++) {
        Chunk* prev = &chunks[l];
        Chunk* next = &chunks[r];
        // Ending char == start char
        if (prev->buf[prev->buf_length - 1] == next->buf[sizeof(int)]) {
            // Update the size of the sequence
            int *prev_count = (int *)(prev->buf + prev->buf_length - (sizeof(int) + 1));
            int *next_count = (int *)(next->buf);
            *prev_count += *next_count;
            
            // Shift the start of the arrays and update the length of the compressed block
            next->buf += sizeof(int) + 1;
            next->buf_length -= (sizeof(int) + 1);
        }
        l = next->buf_length ? r : l;
    }

    for (uint i = 0; i < total_chunks; i++) {
        Chunk *chunk = &chunks[i];
        uint length = chunk->buf_length;
        if (length > 0)
            fwrite(chunk->buf, 1, length, stdout);
    }
    
    free(memory_block);
    
    return 0;
}

void *zip(void *chunks) {
    for (;;) {
        // Get a chunk from the "queue"
        int curr_index = chunks_shared_index++;

        // No more work to do
        if (curr_index >= total_chunks) pthread_exit(NULL);

        // Actual data
        Chunk *chunk = &((Chunk *)chunks)[curr_index];
        uint length = chunk->length;
        char *start = chunk->start;
        char *buf = chunk->buf;
        
        // Run length-encoding
        uint zipped_index = 0, chunk_index = 1;
        while(chunk_index < length) {
            uint streak = 1;
            char curr_byte = start[chunk_index - 1];

            while((chunk_index < length) && (start[chunk_index++] == curr_byte))
                streak++;

            *((uint *)(buf + zipped_index)) = streak;
            buf[zipped_index + sizeof(int)] = curr_byte;
            zipped_index += sizeof(int) + 1;
        }
        // Edge case: last char is not part of a streak
        if (start[length - 1] != start[length - 2]) {
            *((uint *)(buf + zipped_index)) = 1;
            buf[zipped_index + sizeof(int)] = start[length - 1];
            zipped_index += sizeof(int) + 1;
        }
        
        chunk->buf_length = zipped_index; 
    }
}
