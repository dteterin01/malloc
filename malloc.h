//
// Created by Danil on 28.08.2023.
//

#ifndef _MALLOC_H
#define _MALLOC_H 1

#include <unistd.h>

# define MAX_TINY_CHUNK  64
# define MAX_SMALL_CHUNK 1024

# define HEADERS_SIZE    32

# define LAST_CHUNK_FLAG 0b010
# define FIRST_CHUNK_HEADER 0b101
# define FIRST_CHUNK_FLAG 0b100
# define PREVIOUS_CHUNK_USED_FLAG 0b001

#include <bits/pthreadtypes.h>
#include <pthread.h>

typedef struct s_bucket
{
    struct s_bucket *prev;
    struct s_bucket *next;
} t_bucket;

typedef struct s_chunk
{
    size_t	prev_size;
    size_t	size;
} t_chunk;

typedef enum e_zone
{
    TINY,
    SMALL,
    LARGE
}   t_zone;

extern t_bucket *zones[LARGE];

void free(void* ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);

#endif
