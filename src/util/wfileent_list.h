#ifndef __WFILEENT_LIST_H
#define __WFILEENT_LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>  // for ssize_t

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    uint16_t *name;
    bool is_dir;
} wfileent;

typedef struct {
    size_t count;
    size_t capacity;
    wfileent *items;
} wfileent_list;

// Create a new wfileent list with initial capacity
wfileent_list* wfileent_list_create(size_t initial_capacity);

// Free a wfileent list and all its items
void wfileent_list_free(wfileent_list* list);

// Add a wfileent to the end of the list (duplicates the name)
int wfileent_list_add(wfileent_list* list, const uint16_t* name, bool is_dir);

// Append a wfileent to the end (with automatic capacity growth)
int wfileent_list_append(wfileent_list* list, const uint16_t* name, bool is_dir);

// Insert a wfileent at a specific index
int wfileent_list_insert(wfileent_list* list, size_t index, const uint16_t* name, bool is_dir);

// Remove a wfileent at a specific index
int wfileent_list_remove(wfileent_list* list, size_t index);

// Get the wfileent at a specific index (returns NULL if out of bounds)
const wfileent* wfileent_list_get(const wfileent_list* list, size_t index);

// Find the index of a wfileent by name (returns -1 if not found)
ssize_t wfileent_list_find(const wfileent_list* list, const uint16_t* name);

// Check if the list contains a wfileent with the given name
int wfileent_list_contains(const wfileent_list* list, const uint16_t* name);

// Get the number of items in the list
size_t wfileent_list_size(const wfileent_list* list);

// Get the capacity of the list
size_t wfileent_list_capacity(const wfileent_list* list);

// Clear all items from the list (but keep the list structure)
void wfileent_list_clear(wfileent_list* list);

// Resize the capacity of the list
int wfileent_list_resize(wfileent_list* list, size_t new_capacity);

// Sort the list alphabetically by name
void wfileent_list_sort(wfileent_list* list);

// Create a copy of the list
wfileent_list* wfileent_list_copy(const wfileent_list* list);

#ifdef __cplusplus
}
#endif

#endif // __WFILEENT_LIST_H

