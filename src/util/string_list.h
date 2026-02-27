#ifndef __STRING_LIST_H
#define __STRING_LIST_H

#include <stddef.h>
#include <sys/types.h>  // for ssize_t

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    size_t count;
    size_t capacity;
    char **items;
} string_list;

// Create a new string list with initial capacity
string_list* string_list_create(size_t initial_capacity);

// Free a string list and all its items
void string_list_free(string_list* list);

// Add a string to the end of the list (duplicates the string)
int string_list_add(string_list* list, const char* str);

// Append a string to the end (with automatic capacity growth)
int string_list_append(string_list* list, const char* str);

// Insert a string at a specific index
int string_list_insert(string_list* list, size_t index, const char* str);

// Remove a string at a specific index
int string_list_remove(string_list* list, size_t index);

// Get the string at a specific index (returns NULL if out of bounds)
const char* string_list_get(const string_list* list, size_t index);

// Find the index of a string (returns -1 if not found)
ssize_t string_list_find(const string_list* list, const char* str);

// Check if the list contains a string
int string_list_contains(const string_list* list, const char* str);

// Get the number of items in the list
size_t string_list_size(const string_list* list);

// Get the capacity of the list
size_t string_list_capacity(const string_list* list);

// Clear all items from the list (but keep the list structure)
void string_list_clear(string_list* list);

// Resize the capacity of the list
int string_list_resize(string_list* list, size_t new_capacity);

// Sort the list alphabetically
void string_list_sort(string_list* list);

// Join all strings with a separator (returns a new string that must be freed)
char* string_list_join(const string_list* list, const char* separator);

// Create a copy of the list
string_list* string_list_copy(const string_list* list);

// Remove duplicate strings from the list
void string_list_unique(string_list* list);

#ifdef __cplusplus
}
#endif

#endif // __STRING_LIST_H
