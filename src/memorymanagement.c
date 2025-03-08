#include <memorymanagement.h>

typedef struct MemoryChunck
{
    struct MemoryChunck *next;
    struct MemoryChunck *perv;
    bool allocated;
    size_t size;
} MemoryChunck;

MemoryChunck* first;

//initiates heap, start is the address of the start of the heap and size is the size of it
void init_memory(size_t start, size_t size)
{
    if(size < sizeof(MemoryChunck)){
        first = 0;
    } else {
        first = (MemoryChunck*)start;

        first->next = 0;
        first->perv = 0;
        first->allocated = false;
        first->size = size - sizeof(MemoryChunck);
    }
}

/// @brief allocates a memory chunk of a certain size
/// @param size the size of the memory chunk
/// @return if allocation successful a pointer to the chunk, 0 otherwise
void* malloc(size_t size)
{
    MemoryChunck *res = 0;

    for (MemoryChunck* c = first; c != 0 && res == 0; c = c->next)
    {
        if(c->size > size && !c->allocated)
            res = c;
    }

    if (res == 0)
        return 0;
    
    //if there is enough memory, creates a new unallocated memory chunk after the one just alloctaed
    if (res->size >= size + sizeof(MemoryChunck) + 1) {
        MemoryChunck *temp = (MemoryChunck*)((size_t) res + sizeof(MemoryChunck) + size);

        temp->next = res->next;
        temp->perv = res;
        temp->allocated = false;
        temp->size = res->size - size - sizeof(MemoryChunck);
        if (temp->next != 0)
            temp->next->perv = temp;
        
        res->next = temp;
        res->size = size;
    }
    
    res->allocated = true;
    return (void*)(((size_t)res) + sizeof(MemoryChunck));
}

/// @brief allocates memory for a number of object with a specified size
/// @param n the number of objects
/// @param size the size of each
/// @return if allocation successful a pointer to the chunk, 0 otherwise
void* calloc(size_t n, size_t size)
{
    return malloc(n * size);
}

/// @brief frees an allocated memory chunk
/// @param ptr a pointer to the memory chunk, right after descriptor
void free(void* ptr)
{
    MemoryChunck* chunk = ptr - sizeof(MemoryChunck);

    chunk->allocated = false;
    //if the chunk before was not allocated it merges both chuncks
    if (chunk->perv != 0 && !chunk->perv->allocated) {
        chunk->perv->size += chunk->size + sizeof(MemoryChunck);
        chunk->perv->next = chunk->next;
        if (chunk->next != 0)
            chunk->next->perv = chunk->perv;
        
        chunk = chunk->perv;
    }

    //if the chunk after was not allocated it merges both chuncks
    if (chunk->next != 0 && !chunk->next->allocated) {
        chunk->next = chunk->next->next;
        chunk->size += chunk->next->size + sizeof(MemoryChunck);
        if (chunk->next != 0)
            chunk->next->perv = chunk;
    }
}