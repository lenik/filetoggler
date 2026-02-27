#include "wfileent_list.h"
#include "wstring.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>  // for ssize_t

#define WFILEENT_LIST_DEFAULT_CAPACITY 16
#define WFILEENT_LIST_GROWTH_FACTOR 2

// Ensure capacity for at least 'min_capacity' items
static int ensure_capacity(wfileent_list* list, size_t min_capacity) {
    if (!list) return 0;
    
    if (min_capacity <= list->capacity) {
        return 1;  // Already has enough capacity
    }
    
    size_t new_capacity = list->capacity == 0 ? WFILEENT_LIST_DEFAULT_CAPACITY : list->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= WFILEENT_LIST_GROWTH_FACTOR;
    }
    
    wfileent* new_items = realloc(list->items, sizeof(wfileent) * new_capacity);
    if (!new_items) {
        return 0;
    }
    
    // Initialize new slots
    memset(new_items + list->capacity, 0, sizeof(wfileent) * (new_capacity - list->capacity));
    
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

// Create a new wfileent list with initial capacity
wfileent_list* wfileent_list_create(size_t initial_capacity) {
    wfileent_list* list = malloc(sizeof(wfileent_list));
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

// Free a wfileent list and all its items
void wfileent_list_free(wfileent_list* list) {
    if (!list) {
        return;
    }
    
    if (list->items) {
        // Free all name strings
        for (size_t i = 0; i < list->count; i++) {
            if (list->items[i].name) {
                free(list->items[i].name);
            }
        }
        free(list->items);
    }
    
    free(list);
}

// Add a wfileent to the end of the list (duplicates the name)
int wfileent_list_add(wfileent_list* list, const uint16_t* name, bool is_dir) {
    return wfileent_list_append(list, name, is_dir);
}

// Append a wfileent to the end (with automatic capacity growth)
int wfileent_list_append(wfileent_list* list, const uint16_t* name, bool is_dir) {
    if (!list || !name) return 0;
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    uint16_t* name_copy = wstring_dup(name);
    if (!name_copy) {
        return 0;
    }
    
    list->items[list->count].name = name_copy;
    list->items[list->count].is_dir = is_dir;
    list->count++;
    return 1;
}

// Insert a wfileent at a specific index
int wfileent_list_insert(wfileent_list* list, size_t index, const uint16_t* name, bool is_dir) {
    if (!list || !name) return 0;
    if (index > list->count) return 0;  // Can insert at count (append)
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    uint16_t* name_copy = wstring_dup(name);
    if (!name_copy) {
        return 0;
    }
    
    // Shift items to the right
    for (size_t i = list->count; i > index; i--) {
        list->items[i] = list->items[i - 1];
    }
    
    list->items[index].name = name_copy;
    list->items[index].is_dir = is_dir;
    list->count++;
    return 1;
}

// Remove a wfileent at a specific index
int wfileent_list_remove(wfileent_list* list, size_t index) {
    if (!list || index >= list->count) return 0;
    
    if (list->items[index].name) {
        free(list->items[index].name);
    }
    
    // Shift items to the left
    for (size_t i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    
    list->count--;
    // Clear the last slot
    list->items[list->count].name = NULL;
    list->items[list->count].is_dir = false;
    return 1;
}

// Get the wfileent at a specific index (returns NULL if out of bounds)
const wfileent* wfileent_list_get(const wfileent_list* list, size_t index) {
    if (!list || index >= list->count) return NULL;
    return &list->items[index];
}

// Find the index of a wfileent by name (returns -1 if not found)
ssize_t wfileent_list_find(const wfileent_list* list, const uint16_t* name) {
    if (!list || !name) return -1;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name && wstring_cmp(list->items[i].name, name) == 0) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

// Check if the list contains a wfileent with the given name
int wfileent_list_contains(const wfileent_list* list, const uint16_t* name) {
    return wfileent_list_find(list, name) >= 0;
}

// Get the number of items in the list
size_t wfileent_list_size(const wfileent_list* list) {
    return list ? list->count : 0;
}

// Get the capacity of the list
size_t wfileent_list_capacity(const wfileent_list* list) {
    return list ? list->capacity : 0;
}

// Clear all items from the list (but keep the list structure)
void wfileent_list_clear(wfileent_list* list) {
    if (!list) return;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name) {
            free(list->items[i].name);
            list->items[i].name = NULL;
        }
        list->items[i].is_dir = false;
    }
    list->count = 0;
}

// Resize the capacity of the list
int wfileent_list_resize(wfileent_list* list, size_t new_capacity) {
    if (!list) return 0;
    
    if (new_capacity < list->count) {
        // Shrink: free items beyond new capacity
        for (size_t i = new_capacity; i < list->count; i++) {
            if (list->items[i].name) {
                free(list->items[i].name);
            }
        }
        list->count = new_capacity;
    }
    
    wfileent* new_items = realloc(list->items, sizeof(wfileent) * new_capacity);
    if (!new_items && new_capacity > 0) {
        return 0;
    }
    
    list->items = new_items;
    list->capacity = new_capacity;
    
    // Initialize new slots
    if (new_capacity > list->count) {
        memset(list->items + list->count, 0, sizeof(wfileent) * (new_capacity - list->count));
    }
    
    return 1;
}

// Comparison function for sorting
static int wfileent_compare(const void* a, const void* b) {
    const wfileent* ent_a = (const wfileent*)a;
    const wfileent* ent_b = (const wfileent*)b;
    
    if (!ent_a->name && !ent_b->name) return 0;
    if (!ent_a->name) return 1;
    if (!ent_b->name) return -1;
    
    return wstring_cmp(ent_a->name, ent_b->name);
}

// Sort the list alphabetically by name
void wfileent_list_sort(wfileent_list* list) {
    if (!list || list->count == 0) return;
    
    qsort(list->items, list->count, sizeof(wfileent), wfileent_compare);
}

// Create a copy of the list
wfileent_list* wfileent_list_copy(const wfileent_list* list) {
    if (!list) return NULL;
    
    wfileent_list* copy = wfileent_list_create(list->capacity);
    if (!copy) return NULL;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name) {
            if (!wfileent_list_append(copy, list->items[i].name, list->items[i].is_dir)) {
                wfileent_list_free(copy);
                return NULL;
            }
        }
    }
    
    return copy;
}

