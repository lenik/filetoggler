#ifndef __FILEENT_LIST_H
#define __FILEENT_LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>  // for ssize_t

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    char *name;
    bool is_dir;
} fileent;

typedef struct {
    size_t count;
    size_t capacity;
    fileent *items;
} fileent_list;

// Create a new fileent list with initial capacity
fileent_list* fileent_list_create(size_t initial_capacity);

// Free a fileent list and all its items
void fileent_list_free(fileent_list* list);

// Add a fileent to the end of the list (duplicates the name)
int fileent_list_add(fileent_list* list, const char* name, bool is_dir);

// Append a fileent to the end (with automatic capacity growth)
int fileent_list_append(fileent_list* list, const char* name, bool is_dir);

// Insert a fileent at a specific index
int fileent_list_insert(fileent_list* list, size_t index, const char* name, bool is_dir);

// Remove a fileent at a specific index
int fileent_list_remove(fileent_list* list, size_t index);

// Get the fileent at a specific index (returns NULL if out of bounds)
const fileent* fileent_list_get(const fileent_list* list, size_t index);

// Find the index of a fileent by name (returns -1 if not found)
ssize_t fileent_list_find(const fileent_list* list, const char* name);

// Check if the list contains a fileent with the given name
int fileent_list_contains(const fileent_list* list, const char* name);

// Get the number of items in the list
size_t fileent_list_size(const fileent_list* list);

// Get the capacity of the list
size_t fileent_list_capacity(const fileent_list* list);

// Clear all items from the list (but keep the list structure)
void fileent_list_clear(fileent_list* list);

// Resize the capacity of the list
int fileent_list_resize(fileent_list* list, size_t new_capacity);

// Sort the list alphabetically by name
void fileent_list_sort(fileent_list* list);

// Create a copy of the list
fileent_list* fileent_list_copy(const fileent_list* list);

#ifdef __cplusplus
}
#endif

#endif // __FILEENT_LIST_H

