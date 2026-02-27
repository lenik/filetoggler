#include "fileent_list.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>  // for ssize_t

#define FILEENT_LIST_DEFAULT_CAPACITY 16
#define FILEENT_LIST_GROWTH_FACTOR 2

// Ensure capacity for at least 'min_capacity' items
static int ensure_capacity(fileent_list* list, size_t min_capacity) {
    if (!list) return 0;
    
    if (min_capacity <= list->capacity) {
        return 1;  // Already has enough capacity
    }
    
    size_t new_capacity = list->capacity == 0 ? FILEENT_LIST_DEFAULT_CAPACITY : list->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= FILEENT_LIST_GROWTH_FACTOR;
    }
    
    fileent* new_items = realloc(list->items, sizeof(fileent) * new_capacity);
    if (!new_items) {
        return 0;
    }
    
    // Initialize new slots
    memset(new_items + list->capacity, 0, sizeof(fileent) * (new_capacity - list->capacity));
    
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

// Create a new fileent list with initial capacity
fileent_list* fileent_list_create(size_t initial_capacity) {
    fileent_list* list = malloc(sizeof(fileent_list));
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

// Free a fileent list and all its items
void fileent_list_free(fileent_list* list) {
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

// Add a fileent to the end of the list (duplicates the name)
int fileent_list_add(fileent_list* list, const char* name, bool is_dir) {
    return fileent_list_append(list, name, is_dir);
}

// Append a fileent to the end (with automatic capacity growth)
int fileent_list_append(fileent_list* list, const char* name, bool is_dir) {
    if (!list || !name) return 0;
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    char* name_copy = strdup(name);
    if (!name_copy) {
        return 0;
    }
    
    list->items[list->count].name = name_copy;
    list->items[list->count].is_dir = is_dir;
    list->count++;
    return 1;
}

// Insert a fileent at a specific index
int fileent_list_insert(fileent_list* list, size_t index, const char* name, bool is_dir) {
    if (!list || !name) return 0;
    if (index > list->count) return 0;  // Can insert at count (append)
    
    if (!ensure_capacity(list, list->count + 1)) {
        return 0;
    }
    
    char* name_copy = strdup(name);
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

// Remove a fileent at a specific index
int fileent_list_remove(fileent_list* list, size_t index) {
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

// Get the fileent at a specific index (returns NULL if out of bounds)
const fileent* fileent_list_get(const fileent_list* list, size_t index) {
    if (!list || index >= list->count) return NULL;
    return &list->items[index];
}

// Find the index of a fileent by name (returns -1 if not found)
ssize_t fileent_list_find(const fileent_list* list, const char* name) {
    if (!list || !name) return -1;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name && strcmp(list->items[i].name, name) == 0) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

// Check if the list contains a fileent with the given name
int fileent_list_contains(const fileent_list* list, const char* name) {
    return fileent_list_find(list, name) >= 0;
}

// Get the number of items in the list
size_t fileent_list_size(const fileent_list* list) {
    return list ? list->count : 0;
}

// Get the capacity of the list
size_t fileent_list_capacity(const fileent_list* list) {
    return list ? list->capacity : 0;
}

// Clear all items from the list (but keep the list structure)
void fileent_list_clear(fileent_list* list) {
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
int fileent_list_resize(fileent_list* list, size_t new_capacity) {
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
    
    fileent* new_items = realloc(list->items, sizeof(fileent) * new_capacity);
    if (!new_items && new_capacity > 0) {
        return 0;
    }
    
    list->items = new_items;
    list->capacity = new_capacity;
    
    // Initialize new slots
    if (new_capacity > list->count) {
        memset(list->items + list->count, 0, sizeof(fileent) * (new_capacity - list->count));
    }
    
    return 1;
}

// Comparison function for sorting
static int fileent_compare(const void* a, const void* b) {
    const fileent* ent_a = (const fileent*)a;
    const fileent* ent_b = (const fileent*)b;
    
    if (!ent_a->name && !ent_b->name) return 0;
    if (!ent_a->name) return 1;
    if (!ent_b->name) return -1;
    
    return strcmp(ent_a->name, ent_b->name);
}

// Sort the list alphabetically by name
void fileent_list_sort(fileent_list* list) {
    if (!list || list->count == 0) return;
    
    qsort(list->items, list->count, sizeof(fileent), fileent_compare);
}

// Create a copy of the list
fileent_list* fileent_list_copy(const fileent_list* list) {
    if (!list) return NULL;
    
    fileent_list* copy = fileent_list_create(list->capacity);
    if (!copy) return NULL;
    
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name) {
            if (!fileent_list_append(copy, list->items[i].name, list->items[i].is_dir)) {
                fileent_list_free(copy);
                return NULL;
            }
        }
    }
    
    return copy;
}

