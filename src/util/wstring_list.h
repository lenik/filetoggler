#ifndef __WSTRING_LIST_H
#define __WSTRING_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  // for malloc, free
#include <sys/types.h>  // for ssize_t

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    size_t count;
    size_t capacity;
    uint16_t **items;  // Array of wide strings (uint16_t*)
} wstring_list;

// Create a new wide string list with initial capacity
wstring_list* wstring_list_create(size_t initial_capacity);

// Free a wide string list and all its items
void wstring_list_free(wstring_list* list);

// Add a wide string to the end of the list (duplicates the string)
int wstring_list_add(wstring_list* list, const uint16_t* str);

// Append a wide string to the end (with automatic capacity growth)
int wstring_list_append(wstring_list* list, const uint16_t* str);

// Insert a wide string at a specific index
int wstring_list_insert(wstring_list* list, size_t index, const uint16_t* str);

// Remove a wide string at a specific index
int wstring_list_remove(wstring_list* list, size_t index);

// Get the wide string at a specific index (returns NULL if out of bounds)
const uint16_t* wstring_list_get(const wstring_list* list, size_t index);

// Find the index of a wide string (returns -1 if not found)
ssize_t wstring_list_find(const wstring_list* list, const uint16_t* str);

// Check if the list contains a wide string
int wstring_list_contains(const wstring_list* list, const uint16_t* str);

// Get the number of items in the list
size_t wstring_list_size(const wstring_list* list);

// Get the capacity of the list
size_t wstring_list_capacity(const wstring_list* list);

// Clear all items from the list (but keep the list structure)
void wstring_list_clear(wstring_list* list);

// Resize the capacity of the list
int wstring_list_resize(wstring_list* list, size_t new_capacity);

// Sort the list alphabetically (using wide string comparison)
void wstring_list_sort(wstring_list* list);

// Join all wide strings with a separator (returns a new string that must be freed)
uint16_t* wstring_list_join(const wstring_list* list, const uint16_t* separator);

// Create a copy of the list
wstring_list* wstring_list_copy(const wstring_list* list);

// Remove duplicate wide strings from the list
void wstring_list_unique(wstring_list* list);

#ifdef __cplusplus
}
#endif

#endif // __WSTRING_LIST_H

