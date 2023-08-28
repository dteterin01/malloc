//
// Created by Danil on 28.08.2023.
//

#include <sys/mman.h>
#include "malloc.h"

t_bucket *zones[LARGE];

void
*lib_memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *dst_tmp = dst;
    unsigned char *src_tmp = (unsigned char*)src;

    while (n--)
        dst_tmp[n] = src_tmp[n];

    return dst;
}

t_bucket
*first_chunk_get_bucket(t_chunk *chunk)
{
    return ((t_bucket*)((size_t*)chunk - 1));
}

size_t
chunk_remove_flags(size_t size)
{
    return ((size >> 3) << 3);
}

t_chunk
*first_chunk_from_bucket(t_bucket *bucket)
{
    return ((t_chunk *)((size_t*)bucket +1));
}

t_chunk
*next_chunk(t_chunk* chunk)
{
    return ((t_chunk*)(size_t*)chunk + chunk->prev_size / 8 + 1);
}

t_chunk
*prev_chunk(t_chunk *chunk)
{
    return ((t_chunk*)((size_t*)chunk - chunk->prev_size / 8 - 1));
}

t_chunk
*mem_to_chunk_ptr(size_t *mem)
{
    return ((t_chunk*)mem - 1);
}

int
find_zone(t_bucket *bucket)
{
    int i;

    i = 0;
    while (bucket->prev)
    {
        bucket = bucket->prev;
    }
    while (zones[i] != bucket)
    {
        i++;
    }
    return (i);
}

t_bucket
*remove_bucket_from_zone(t_bucket *bucket, int zone)
{
    t_bucket *prev_bucket = bucket->prev;
    t_bucket *next_bucket = bucket->next;

    if (prev_bucket && next_bucket)
    {
        prev_bucket->next = next_bucket;
        next_bucket->prev = prev_bucket;
    }
    else if (!prev_bucket)
    {
        zones[zone] = bucket->next;
        if (bucket->next)
            next_bucket->prev = NULL;
    }
    else
    {
        prev_bucket->next = NULL;
    }
    return bucket;
}

t_bucket
*add_bucket_to_zone(t_bucket* bucket, int zone)
{
    t_bucket * head_bucket;

    bucket->prev = 0;
    bucket->next = zones[zone];
    head_bucket = zones[zone];
    if (head_bucket)
        head_bucket->prev = bucket;
    zones[zone] = bucket;
    return bucket;
}

int
bucket_is_empty(t_chunk *chunk, t_chunk *next_chunk)
{
    if (chunk->size & FIRST_CHUNK_FLAG && next_chunk->size & LAST_CHUNK_FLAG)
    {
        return 1;
    }
    return 0;
}

int
create_bucket(int zone)
{
    t_bucket * bucket;
    int        bucket_size[2];
    t_chunk *  last_chunk;

    bucket_size[0] = (MAX_TINY_CHUNK + 8) * 100;
    bucket_size[1] = (MAX_SMALL_CHUNK + 8) * 100;
    bucket = (t_bucket*)((size_t*) malloc(bucket_size[zone]) - 3);

    remove_bucket_from_zone(bucket, LARGE);
    add_bucket_to_zone(bucket, zone);

    last_chunk = next_chunk(first_chunk_from_bucket(bucket));
    last_chunk->size = LAST_CHUNK_FLAG;
    return (0);
}

void
*set_chunk_header(t_chunk* chunk, size_t size, int zone)
{
    int		min_chunk[2];
    size_t	first_size;
    t_chunk *next;

    min_chunk[0] = 8 + 8;
    min_chunk[1] = MAX_TINY_CHUNK + 8 + 8;

    if (chunk->size >= size + min_chunk[zone])
    {
        first_size = chunk->size;
        chunk->size = first_size - size - 8;
        next = next_chunk(chunk);
        next->size = size;
        next->prev_size = chunk->size;
        chunk = next;
        next = next_chunk(chunk);
        next->size |= PREVIOUS_CHUNK_USED_FLAG;
    }
    else
    {
        next = next_chunk(chunk);
        next->size |= PREVIOUS_CHUNK_USED_FLAG;
    }
    return ((size_t*)chunk + 2);
}

void
*search_in_zone(size_t size, int zone)
{
    t_chunk  *chunk;
    t_bucket *bucket = zones[zone];

    while (bucket)
    {
        chunk = first_chunk_from_bucket(bucket);
        while ((chunk->size & LAST_CHUNK_FLAG) == 0)
        {
            if (chunk->size >= size && next_chunk(chunk)->size % 2 == 0)
                return set_chunk_header(chunk, size, zone);
            chunk = next_chunk(chunk);
        }

        bucket = bucket->next;
    }
    return NULL;
}

void
*lib_mmap(size_t size)
{
    size_t n_all;
    size_t page_size  = sysconf(_SC_PAGESIZE);
    size_t final_size = size + HEADERS_SIZE;

    void   *memory;

    n_all = page_size * ((final_size / page_size) - (size % page_size == 0) + 1);
    memory = mmap(NULL, n_all, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    return ((size_t*)memory);
}

void
*set_bucket_headers(void * memory, size_t size)
{
    t_bucket * head_bucket = add_bucket_to_zone((t_bucket *)memory, LARGE);;
    t_chunk	*first_chunk = first_chunk_from_bucket(head_bucket);
    first_chunk->size = chunk_remove_flags(size) + 0b101;
    t_chunk	*last_chunk = next_chunk(first_chunk);
    last_chunk->size = LAST_CHUNK_FLAG;

    return ((size_t*)(first_chunk + 1));
}

void
*find_space(size_t size, int zone)
{
    void *allocated;

    if (zone == LARGE)
        return (set_bucket_headers(lib_mmap(size), size));
    allocated = search_in_zone(size, zone);
    if (allocated)
    {
        return allocated;
    }
    create_bucket(zone);
    return find_space(size, zone);
}

void
*malloc(size_t size)
{
    int zone;

    if (size == 0)
    {
        return 0;
    }

    zone = (size > MAX_TINY_CHUNK) + (size > MAX_SMALL_CHUNK);

    size = size + (size % 8 != 0 ? 8 - size % 8 : 0);
    return find_space(size, zone);
}

void
free_bucket(void *ptr, size_t size, int zone)
{
    t_bucket *bucket = (t_bucket*)((size_t*) ptr - 3);
    remove_bucket_from_zone(bucket, zone);
    munmap((size_t*)ptr - 3, size);
}

void
free_chunk(t_chunk * chunk)
{
    t_chunk *next = next_chunk(chunk);
    t_chunk *prev = prev_chunk(chunk);

    next->size &= ~(1UL);
    next->prev_size = chunk_remove_flags(chunk->size);

    if (!(next->size & LAST_CHUNK_FLAG) && next_chunk(next)->size % 2 == 0)
        chunk->size += chunk_remove_flags(next->size) + 8;
    if (chunk->size % 2 == 0 && !(chunk->size & FIRST_CHUNK_FLAG))
    {
        prev->size += chunk_remove_flags(chunk->size) + 8;
        chunk = prev;
    }
    if (bucket_is_empty(chunk, next_chunk(chunk)))
    {
        free_bucket((void *)((size_t*)(chunk) + 1), chunk->size, find_zone(first_chunk_get_bucket(chunk)));
    }
}

void
free(void *ptr)
{
    if (ptr == NULL)
        return;

    t_chunk *chunk = mem_to_chunk_ptr(ptr);
    t_chunk *next = next_chunk(chunk);

    if ((next->size & 0b1) == 0)
    {
        return;
    }

    if (chunk->size > MAX_SMALL_CHUNK)
    {
        free_bucket(ptr, chunk->size, LARGE);
    }
    free_chunk(chunk);
}

void
*realloc(void *ptr, size_t size)
{
    size_t	current_size = chunk_remove_flags(mem_to_chunk_ptr(ptr)->size);

    if (current_size >= size)
        return (ptr);

    if (current_size > MAX_SMALL_CHUNK)
    {
        if ((current_size + 32 - 8) / 4096 == (size + 32) / 4096)
            return (ptr);
    }

    void * new_ptr = lib_memcpy(malloc(size), ptr, current_size);
    free(ptr);
    return new_ptr;
}