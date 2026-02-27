#ifndef __PREFIX_MAP_H
#define __PREFIX_MAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Opaque prefix map structure
typedef struct prefix_map prefix_map;

// Value destruction function type
typedef void (*prefix_map_value_destroy_fn)(void* value);

// Create a new prefix map (uses free() as default value destroy function)
prefix_map* prefix_map_create(void);

// Create a new prefix map with custom value destroy function
// destroy_fn: function to call when a value is removed/freed, or NULL to skip destruction
prefix_map* prefix_map_create_ex(prefix_map_value_destroy_fn destroy_fn);

// Free a prefix map and all its entries
void prefix_map_free(prefix_map* map);

// Add a key-value pair to the prefix map
// If key already exists, the value is replaced
void prefix_map_add(prefix_map* map, const char* key, void* value);

// Find a value by exact key match
void* prefix_map_get(const prefix_map* map, const char* key);

// Find a value by prefix match (returns first match found)
void* prefix_map_find_prefix(const prefix_map* map, const char* prefix);

// Remove a key from the prefix map
int prefix_map_remove(prefix_map* map, const char* key);

// Check if a key exists in the map
int prefix_map_contains(const prefix_map* map, const char* key);

// Get the number of entries in the map
size_t prefix_map_size(const prefix_map* map);

// Clear the prefix map (free all entries but keep the map)
void prefix_map_clear(prefix_map* map);

// Iterate over all entries in the map
// callback(key, value, user_data) - return non-zero to stop iteration
void prefix_map_foreach(const prefix_map* map, 
                       int (*callback)(const char* key, void* value, void* user_data),
                       void* user_data);

#ifdef __cplusplus
}
#endif

#endif // __PREFIX_MAP_H
