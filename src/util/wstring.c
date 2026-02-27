#include "wstring.h"
#include <stdlib.h>
#include <string.h>

// Calculate length of a wide string (null-terminated)
size_t wstring_len(const uint16_t* str) {
    if (!str) return 0;
    size_t len = 0;
    while (str[len] != 0) len++;
    return len;
}

// Compare two wide strings (returns <0, 0, or >0)
int wstring_cmp(const uint16_t* a, const uint16_t* b) {
    if (!a && !b) return 0;
    if (!a) return 1;
    if (!b) return -1;
    
    size_t i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
        i++;
    }
    if (a[i] == 0 && b[i] == 0) return 0;
    if (a[i] == 0) return -1;
    return 1;
}

// Find a wide string in another (wide strstr)
const uint16_t* wstring_strstr(const uint16_t* haystack, const uint16_t* needle) {
    if (!haystack || !needle || !needle[0]) return haystack;
    
    size_t needle_len = wstring_len(needle);
    size_t haystack_len = wstring_len(haystack);
    
    if (needle_len > haystack_len) return NULL;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (haystack[i + j] != needle[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            return haystack + i;
        }
    }
    return NULL;
}

// Duplicate a wide string (returns new string that must be freed)
uint16_t* wstring_dup(const uint16_t* str) {
    if (!str) return NULL;
    size_t len = wstring_len(str);
    uint16_t* copy = malloc((len + 1) * sizeof(uint16_t));
    if (!copy) return NULL;
    for (size_t i = 0; i <= len; i++) {
        copy[i] = str[i];
    }
    return copy;
}

// Convert ASCII string to wide string
void wstring_from_ascii(const char* ascii, uint16_t* wide, size_t max_len) {
    if (!ascii || !wide || max_len == 0) {
        if (wide && max_len > 0) {
            wide[0] = 0;
        }
        return;
    }
    
    size_t i = 0;
    while (ascii[i] != 0 && i < max_len - 1) {
        wide[i] = (uint16_t)(unsigned char)ascii[i];
        i++;
    }
    wide[i] = 0;
}

