#ifndef UTIL_H_20130921
#define UTIL_H_20130921
#include <stddef.h>

#define MAX_MSG_CHARS (32)

void *memcpy(void *dest, const void *src, size_t n);
void  my_puts(char *s);
void  my_print(char *msg);
void  my_printf(const char *fmt_str, ...);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

char* itoa(int val);

#endif /* UTIL_H_20130921 */
