#include "prefix_map.h"
#include <stdlib.h>
#include <string.h>

// Hash table entry for prefix map
typedef struct prefix_map_entry {
    char* key;
    void* value;
    struct prefix_map_entry* next;  // For collision chaining
} entry_t;

#define PREFIX_MAP_SIZE 128  // Hash table size

// Prefix map structure
struct prefix_map {
    entry_t* buckets[PREFIX_MAP_SIZE];
    size_t count;
    prefix_map_value_destroy_fn value_destroy;
};

// Simple hash function for strings
static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % PREFIX_MAP_SIZE;
}

// Create a new prefix map (uses free() as default value destroy function)
prefix_map* prefix_map_create(void) {
    return prefix_map_create_ex(free);
}

// Create a new prefix map with custom value destroy function
prefix_map* prefix_map_create_ex(prefix_map_value_destroy_fn destroy_fn) {
    prefix_map* map = calloc(1, sizeof(prefix_map));
    if (map) {
        map->value_destroy = destroy_fn;
    }
    return map;
}

// Free a prefix map and all its entries
void prefix_map_free(prefix_map* map) {
    if (!map) return;
    
    prefix_map_clear(map);
    free(map);
}

// Add a key-value pair to the prefix map
// If key already exists, the value is replaced
void prefix_map_add(prefix_map* map, const char* key, void* value) {
    if (!map || !key) return;
    
    unsigned int hash = hash_string(key);
    
    // Check if key already exists
    entry_t* existing = map->buckets[hash];
    while (existing) {
        if (strcmp(existing->key, key) == 0) {
            // Key exists - destroy old value and replace
            if (map->value_destroy && existing->value) {
                map->value_destroy(existing->value);
            }
            existing->value = value;
            return;
        }
        existing = existing->next;
    }
    
    // Key doesn't exist, add it
    entry_t* entry = malloc(sizeof(entry_t));
    if (!entry) return;
    
    entry->key = strdup(key);
    entry->value = value;
    entry->next = map->buckets[hash];
    map->buckets[hash] = entry;
    map->count++;
}

// Find a value by exact key match
void* prefix_map_get(const prefix_map* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int hash = hash_string(key);
    entry_t* entry = map->buckets[hash];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

// Find a value by prefix match (returns first match found)
void* prefix_map_find_prefix(const prefix_map* map, const char* prefix) {
    if (!map || !prefix) return NULL;
    
    int prefix_len = strlen(prefix);
    void* best_match = NULL;
    int best_match_len = 0;
    
    // Search all buckets for prefix matches
    for (int i = 0; i < PREFIX_MAP_SIZE; i++) {
        entry_t* entry = map->buckets[i];
        while (entry) {
            int key_len = strlen(entry->key);
            // Check if prefix matches
            if (prefix_len <= key_len && strncmp(entry->key, prefix, prefix_len) == 0) {
                // Prefer exact matches, then shorter keys
                if (!best_match || 
                    (key_len == prefix_len) ||  // Exact match
                    (best_match_len > key_len && key_len >= prefix_len)) {
                    best_match = entry->value;
                    best_match_len = key_len;
                }
            }
            entry = entry->next;
        }
    }
    
    return best_match;
}

// Remove a key from the prefix map
int prefix_map_remove(prefix_map* map, const char* key) {
    if (!map || !key) return 0;
    
    unsigned int hash = hash_string(key);
    entry_t* entry = map->buckets[hash];
    entry_t* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[hash] = entry->next;
            }
            
            // Destroy value if destroy function is set
            if (map->value_destroy && entry->value) {
                map->value_destroy(entry->value);
            }
            
            free(entry->key);
            free(entry);
            map->count--;
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return 0;
}

// Check if a key exists in the map
int prefix_map_contains(const prefix_map* map, const char* key) {
    return prefix_map_get(map, key) != NULL;
}

// Get the number of entries in the map
size_t prefix_map_size(const prefix_map* map) {
    return map ? map->count : 0;
}

// Clear the prefix map (free all entries but keep the map)
void prefix_map_clear(prefix_map* map) {
    if (!map) return;
    
    for (int i = 0; i < PREFIX_MAP_SIZE; i++) {
        entry_t* entry = map->buckets[i];
        while (entry) {
            entry_t* next = entry->next;
            
            // Destroy value if destroy function is set
            if (map->value_destroy && entry->value) {
                map->value_destroy(entry->value);
            }
            
            free(entry->key);
            free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->count = 0;
}

// Iterate over all entries in the map
void prefix_map_foreach(const prefix_map* map, 
                       int (*callback)(const char* key, void* value, void* user_data),
                       void* user_data) {
    if (!map || !callback) return;
    
    for (int i = 0; i < PREFIX_MAP_SIZE; i++) {
        entry_t* entry = map->buckets[i];
        while (entry) {
            if (callback(entry->key, entry->value, user_data) != 0) {
                return;  // Stop iteration if callback returns non-zero
            }
            entry = entry->next;
        }
    }
}
