#include "wstring_list.h"
#include "wstring.h"
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>  // for ssize_t

#define WSTRING_LIST_DEFAULT_CAPACITY 16
#define WSTRING_LIST_GROWTH_FACTOR 2


// Ensure capacity for at least 'min_capacity' items
static int ensure_capacity(wstring_list* list, size_t min_capacity) {
    if (!list) return 0;
    
    if (min_capacity <= list->capacity) {
        return 1;  // Already has enough capacity
    }
    
    size_t new_capacity = list->capacity == 0 ? WSTRING_LIST_DEFAULT_CAPACITY : list->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= WSTRING_LIST_GROWTH_FACTOR;
    }
    
    uint16_t** new_items = realloc(list->items, sizeof(uint16_t*) * new_capacity);
    if (!new_items) {
        return 0;
    }
    
    // Initialize new slots to NULL
    for (size_t i = list->capacity; i < new_capacity; i++) {
        new_items[i] = NULL;
    }
    
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

// Create a new wide string list with initial capacity
wstring_list* wstring_list_create(size_t initial_capacity) {
    wstring_list* list = malloc(sizeof(wstring_list));
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

// Free a wide string list and all its items
void wstring_list_free(wstring_list* list) {
    if (!list) {
        return;
    }
    
    if (list->items) {
        // Free all wide string items
        for (size_t i = 0; i < list->count; i++) {
            if (list->items[i]) {
                free(list->items[i]);
            }
        }
        free(list->items);
    }
    
    free(list);
}

// Add a wide string to the end of the list (duplicates the string)
int wstring_list_add(wstring_list* list, const uint16_t* str) {
    return wstring_list_append(list, str);
}

// Append a wide string to the end (with automatic capacity growth)
int wstring_list_append(wstring_list* list, const uint16_t* str) {
    if (!list || !str) return 0;
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    uint16_t* copy = wstring_dup(str);
    if (!copy) {
        return 0;
    }
    
    list->items[list->count++] = copy;
    return 1;
}

// Insert a wide string at a specific index
int wstring_list_insert(wstring_list* list, size_t index, const uint16_t* str) {
    if (!list || !str) return 0;
    if (index > list->count) return 0;  // Can insert at count (append)
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    uint16_t* copy = wstring_dup(str);
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

// Remove a wide string at a specific index
int wstring_list_remove(wstring_list* list, size_t index) {
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

// Get the wide string at a specific index (returns NULL if out of bounds)
const uint16_t* wstring_list_get(const wstring_list* list, size_t index) {
    if (!list || index >= list->count) return NULL;
    return list->items[index];
}

// Find the index of a wide string (returns -1 if not found)
ssize_t wstring_list_find(const wstring_list* list, const uint16_t* str) {
    if (!list || !str) return -1;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i] && wstring_cmp(list->items[i], str) == 0) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

// Check if the list contains a wide string
int wstring_list_contains(const wstring_list* list, const uint16_t* str) {
    return wstring_list_find(list, str) >= 0;
}

// Get the number of items in the list
size_t wstring_list_size(const wstring_list* list) {
    return list ? list->count : 0;
}

// Get the capacity of the list
size_t wstring_list_capacity(const wstring_list* list) {
    return list ? list->capacity : 0;
}

// Clear all items from the list (but keep the list structure)
void wstring_list_clear(wstring_list* list) {
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
int wstring_list_resize(wstring_list* list, size_t new_capacity) {
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
    
    uint16_t** new_items = realloc(list->items, sizeof(uint16_t*) * new_capacity);
    if (!new_items && new_capacity > 0) {
        return 0;
    }
    
    list->items = new_items;
    list->capacity = new_capacity;
    
    // Initialize new slots to NULL
    for (size_t i = list->count; i < new_capacity; i++) {
        list->items[i] = NULL;
    }
    
    return 1;
}

// Comparison function for sorting
static int wstring_compare(const void* a, const void* b) {
    const uint16_t* str_a = *(const uint16_t**)a;
    const uint16_t* str_b = *(const uint16_t**)b;
    
    if (!str_a && !str_b) return 0;
    if (!str_a) return 1;
    if (!str_b) return -1;
    
    return wstring_cmp(str_a, str_b);
}

// Sort the list alphabetically
void wstring_list_sort(wstring_list* list) {
    if (!list || list->count == 0) return;
    
    qsort(list->items, list->count, sizeof(uint16_t*), wstring_compare);
}

// Join all wide strings with a separator (returns a new string that must be freed)
uint16_t* wstring_list_join(const wstring_list* list, const uint16_t* separator) {
    if (!list || list->count == 0) {
        uint16_t* empty = malloc(sizeof(uint16_t));
        if (empty) empty[0] = 0;
        return empty;
    }
    
    if (!separator) {
        separator = (const uint16_t*)L"";
    }
    
    // Calculate total length needed
    size_t total_len = 0;
    size_t sep_len = separator ? wstring_len(separator) : 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            total_len += wstring_len(list->items[i]);
        }
    }
    total_len += sep_len * (list->count > 0 ? list->count - 1 : 0);
    total_len += 1;  // null terminator
    
    uint16_t* result = malloc(total_len * sizeof(uint16_t));
    if (!result) return NULL;
    
    size_t pos = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            if (i > 0 && sep_len > 0) {
                // Copy separator
                for (size_t j = 0; j < sep_len; j++) {
                    result[pos++] = separator[j];
                }
            }
            // Copy item
            size_t item_len = wstring_len(list->items[i]);
            for (size_t j = 0; j < item_len; j++) {
                result[pos++] = list->items[i][j];
            }
        }
    }
    result[pos] = 0;  // Null terminator
    
    return result;
}

// Create a copy of the list
wstring_list* wstring_list_copy(const wstring_list* list) {
    if (!list) return NULL;
    
    wstring_list* copy = wstring_list_create(list->capacity);
    if (!copy) return NULL;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i]) {
            if (!wstring_list_append(copy, list->items[i])) {
                wstring_list_free(copy);
                return NULL;
            }
        }
    }
    
    return copy;
}

// Remove duplicate wide strings from the list
void wstring_list_unique(wstring_list* list) {
    if (!list || list->count <= 1) return;
    
    // Sort first to group duplicates together
    wstring_list_sort(list);
    
    // Remove duplicates
    size_t write_idx = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (i == 0 || (list->items[i] && list->items[write_idx - 1] && 
                      wstring_cmp(list->items[i], list->items[write_idx - 1]) != 0)) {
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

