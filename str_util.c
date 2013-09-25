#include "str_util.h"
#include "stm32f10x.h"
#include "RTOSConfig.h"

void my_puts(char *s)
{
    while (*s) {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
            /* wait */ ;
        USART_SendData(USART2, *s);
        s++;
    }
}

/* Hand made tools */
int strncmp(const char *str_a, const char *str_b, size_t n)
{
    int i = 0;

    for(i = 0; i < n; i++) {
        if (str_a[i] != str_b[i]) {
            return str_a[i] - str_b[i];
        }
    }
    return 0;
}

char* itoa(int val)
{
    static char buf[32] = { 0 };
    char has_minus = 0;
    int i = 30;

    if (val == 0) {
        buf[1] = '0';
        return &buf[1];
    }

    if (val < 0) {
        val = val * -1;
        has_minus = 1;
    }

    for (; val && (i - 1) ; --i, val /= 10)
        buf[i] = "0123456789"[val % 10];

    if (has_minus) {
        buf[i] = '-';
        return &buf[i];
    }
    return &buf[i + 1];
}

void my_print(char *msg)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);

    if (!msg) {
        return;
    }

    write(fdout, msg, strlen(msg) + 1);
}

#ifndef USE_ASM_OPTI_FUNC
int strcmp(const char *str_a, const char *str_b)
{
    int i = 0;

    while(str_a[i]) {
        if (str_a[i] != str_b[i]) {
            return str_a[i] - str_b[i];
        }
        i++;
    }
    return 0;
}

size_t strlen(const char *string)
{
    int chars = 0;

    while(*string++) {
        chars++;
    }
    return chars;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    int i;

    for (i = 0; i < n; i++) {
        *((char *)dest + i) = *((char *)src + i);
    }
    return dest;
}
#else /* ASM optimized version */
int strcmp(const char *a, const char *b) __attribute__ ((naked));
int strcmp(const char *a, const char *b)
{
    asm(
        "strcmp_lop:                \n"
        "   ldrb    r2, [r0],#1     \n"
        "   ldrb    r3, [r1],#1     \n"
        "   cmp     r2, #1          \n"
        "   it      hi              \n"
        "   cmphi   r2, r3          \n"
        "   beq     strcmp_lop      \n"
        "    sub     r0, r2, r3      \n"
        "   bx      lr              \n"
        :::
    );
}

size_t strlen(const char *s) __attribute__ ((naked));
size_t strlen(const char *s)
{
    asm(
        "    sub  r3, r0, #1            \n"
        "strlen_loop:               \n"
        "    ldrb r2, [r3, #1]!        \n"
        "    cmp  r2, #0                \n"
        "   bne  strlen_loop        \n"
        "    sub  r0, r3, r0            \n"
        "    bx   lr                    \n"
        :::
    );
}
#endif /* USE_ASM_OPTI_FUNC */
