#include "string_list.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>  // for ssize_t

#define STRING_LIST_DEFAULT_CAPACITY 16
#define STRING_LIST_GROWTH_FACTOR 2

// Ensure capacity for at least 'min_capacity' items
static int ensure_capacity(string_list* list, size_t min_capacity) {
    if (!list) return 0;
    
    if (min_capacity <= list->capacity) {
        return 1;  // Already has enough capacity
    }
    
    size_t new_capacity = list->capacity == 0 ? STRING_LIST_DEFAULT_CAPACITY : list->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= STRING_LIST_GROWTH_FACTOR;
    }
    
    char** new_items = realloc(list->items, sizeof(char*) * new_capacity);
    if (!new_items) {
        return 0;
    }
    
    // Initialize new slots to NULL
    memset(new_items + list->capacity, 0, sizeof(char*) * (new_capacity - list->capacity));
    
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

// Create a new string list with initial capacity
string_list* string_list_create(size_t initial_capacity) {
    string_list* list = malloc(sizeof(string_list));
    if (!list) {
        return NULL;
    }
    
    list->count = 0;
    list->capacity = 0;
    list->items = NULL;
    
    if (initial_capacity > 0) {
        if (!ensure_capacity(list, initial_capacity)) {
            free(list);
            return NULL;
        }
    }
    
    return list;
}

// Free a string list and all its items
void string_list_free(string_list* list) {
    if (!list) {
        return;
    }
    
    if (list->items) {
        // Free all string items
        for (size_t i = 0; i < list->count; i++) {
            if (list->items[i]) {
                free(list->items[i]);
            }
        }
        free(list->items);
    }
    
    free(list);
}

// Add a string to the end of the list (duplicates the string)
int string_list_add(string_list* list, const char* str) {
    return string_list_append(list, str);
}

// Append a string to the end (with automatic capacity growth)
int string_list_append(string_list* list, const char* str) {
    if (!list || !str) return 0;
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    char* copy = strdup(str);
    if (!copy) {
        return 0;
    }
    
    list->items[list->count++] = copy;
    return 1;
}

// Insert a string at a specific index
int string_list_insert(string_list* list, size_t index, const char* str) {
    if (!list || !str) return 0;
    if (index > list->count) return 0;  // Can insert at count (append)
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    char* copy = strdup(str);
    if (!copy) {
        return 0;
    }
    
    // Shift items to the right
    for (size_t i = list->count; i > index; i--) {
        list->items[i] = list->items[i - 1];
    }
    
    list->items[index] = copy;
    list->count++;
    return 1;
}

// Remove a string at a specific index
int string_list_remove(string_list* list, size_t index) {
    if (!list || index >= list->count) return 0;
    
    if (list->items[index]) {
        free(list->items[index]);
    }
    
    // Shift items to the left
    for (size_t i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    
    list->count--;
    list->items[list->count] = NULL;  // Clear the last slot
    return 1;
}

// Get the string at a specific index (returns NULL if out of bounds)
const char* string_list_get(const string_list* list, size_t index) {
    if (!list || index >= list->count) return NULL;
    return list->items[index];
}

// Find the index of a string (returns -1 if not found)
ssize_t string_list_find(const string_list* list, const char* str) {
    if (!list || !str) return -1;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i] && strcmp(list->items[i], str) == 0) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

// Check if the list contains a string
int string_list_contains(const string_list* list, const char* str) {
    return string_list_find(list, str) >= 0;
}

// Get the number of items in the list
size_t string_list_size(const string_list* list) {
    return list ? list->count : 0;
}

// Get the capacity of the list
size_t string_list_capacity(const string_list* list) {
    return list ? list->capacity : 0;
}

// Clear all items from the list (but keep the list structure)
void string_list_clear(string_list* list) {
    if (!list) return;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            free(list->items[i]);
            list->items[i] = NULL;
        }
    }
    list->count = 0;
}

// Resize the capacity of the list
int string_list_resize(string_list* list, size_t new_capacity) {
    if (!list) return 0;
    
    if (new_capacity < list->count) {
        // Shrink: free items beyond new capacity
        for (size_t i = new_capacity; i < list->count; i++) {
            if (list->items[i]) {
                free(list->items[i]);
            }
        }
        list->count = new_capacity;
    }
    
    char** new_items = realloc(list->items, sizeof(char*) * new_capacity);
    if (!new_items && new_capacity > 0) {
        return 0;
    }
    
    list->items = new_items;
    list->capacity = new_capacity;
    
    // Initialize new slots to NULL
    if (new_capacity > list->count) {
        memset(list->items + list->count, 0, sizeof(char*) * (new_capacity - list->count));
    }
    
    return 1;
}

// Comparison function for sorting
static int string_compare(const void* a, const void* b) {
    const char* str_a = *(const char**)a;
    const char* str_b = *(const char**)b;
    
    if (!str_a && !str_b) return 0;
    if (!str_a) return 1;
    if (!str_b) return -1;
    
    return strcmp(str_a, str_b);
}

// Sort the list alphabetically
void string_list_sort(string_list* list) {
    if (!list || list->count == 0) return;
    
    qsort(list->items, list->count, sizeof(char*), string_compare);
}

// Join all strings with a separator (returns a new string that must be freed)
char* string_list_join(const string_list* list, const char* separator) {
    if (!list || list->count == 0) {
        return strdup("");
    }
    
    if (!separator) separator = "";
    
    // Calculate total length needed
    size_t total_len = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            total_len += strlen(list->items[i]);
        }
    }
    total_len += strlen(separator) * (list->count > 0 ? list->count - 1 : 0);
    total_len += 1;  // null terminator
    
    char* result = malloc(total_len);
    if (!result) return NULL;
    
    result[0] = '\0';
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            if (i > 0 && separator[0] != '\0') {
                strcat(result, separator);
            }
            strcat(result, list->items[i]);
        }
    }
    
    return result;
}

// Create a copy of the list
string_list* string_list_copy(const string_list* list) {
    if (!list) return NULL;
    
    string_list* copy = string_list_create(list->capacity);
    if (!copy) return NULL;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            if (!string_list_append(copy, list->items[i])) {
                string_list_free(copy);
                return NULL;
            }
        }
    }
    
    return copy;
}

// Remove duplicate strings from the list
void string_list_unique(string_list* list) {
    if (!list || list->count <= 1) return;
    
    // Sort first to group duplicates together
    string_list_sort(list);
    
    // Remove duplicates
    size_t write_idx = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (i == 0 || (list->items[i] && list->items[write_idx - 1] && 
                      strcmp(list->items[i], list->items[write_idx - 1]) != 0)) {
            if (write_idx != i) {
                // Move item to write position
                if (list->items[write_idx]) {
                    free(list->items[write_idx]);
                }
                list->items[write_idx] = list->items[i];
                list->items[i] = NULL;
            }
            write_idx++;
        } else {
            // Duplicate - free it
            if (list->items[i]) {
                free(list->items[i]);
                list->items[i] = NULL;
            }
        }
    }
    
    list->count = write_idx;
}
