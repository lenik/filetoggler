#ifndef __WSTRING_H
#define __WSTRING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Calculate length of a wide string (null-terminated)
size_t wstring_len(const uint16_t* str);

// Compare two wide strings (returns <0, 0, or >0)
int wstring_cmp(const uint16_t* a, const uint16_t* b);

// Find a wide string in another (wide strstr)
const uint16_t* wstring_strstr(const uint16_t* haystack, const uint16_t* needle);

// Duplicate a wide string (returns new string that must be freed)
uint16_t* wstring_dup(const uint16_t* str);

// Convert ASCII string to wide string
void wstring_from_ascii(const char* ascii, uint16_t* wide, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // __WSTRING_H

