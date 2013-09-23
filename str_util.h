#ifndef UTIL_H_20130921
#define UTIL_H_20130921
#include <stddef.h>

#define MAX_MSG_CHARS (32)

#define RT_OK  (0)
#define RT_ERR (1)

void *memcpy(void *dest, const void *src, size_t n);
void  my_puts(char *s);
void  my_printf(char *msg);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

char* itoa(int val);

#endif /* UTIL_H_20130921 */
