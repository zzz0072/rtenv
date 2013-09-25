#include <stdarg.h> /* For variable list */
#include "str_util.h"
#include "stm32f10x.h"
#include "RTOSConfig.h"

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

#define itoa(val) itoa_base(val, 10)
#define htoa(val) itoa_base(val, 16)

static char* itoa_base(int val, int base)
{
    static char buf[32] = { 0 };
    char has_minus = 0;
    int i = 30;

    /* Sepecial case: 0 */
    if (val == 0) {
        buf[1] = '0';
        return &buf[1];
    }

    if (val < 0) {
        val = -val;
        has_minus = 1;
    }

    for (; val && (i - 1) ; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];

    if (has_minus) {
        buf[i] = '-';
        return &buf[i];
    }
    return &buf[i + 1];
}

void my_puts(char *msg)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);

    if (!msg) {
        return;
    }

    write(fdout, msg, strlen(msg) + 1);
}

void my_printf(const char *fmt_str, ...)
{
    va_list param = {0};

    char *param_str = 0;
    char  param_chr[] = {0, 0}; 
    int   param_int = 0;

    char *str_to_output = 0;
    int   curr_char  = 0;
    int   fdout = mq_open("/tmp/mqueue/out", 0);

    va_start(param, fmt_str);

    /* Let's parse */
    while (fmt_str[curr_char]) {
        /* Deal with normal string
         * increase index by 1 here  */
        if (fmt_str[curr_char++] != '%') {
            param_chr[0]  = fmt_str[curr_char - 1];
            str_to_output = param_chr;
        }
        /* % case-> retrive latter params */
        else {
            switch (fmt_str[curr_char]) {
                case 'S':
                case 's':
                    {
                        str_to_output = va_arg(param, char *);
                    }
                    break;

                case 'd':
                case 'D':
                    {
                       param_int     = va_arg(param, int);
                       str_to_output = itoa(param_int);
                    }
                    break;

                case 'X':
                case 'x':
                    {
                       param_int     = va_arg(param, int);
                       str_to_output = htoa(param_int);
                    }
                    break;

                case 'c':
                case 'C':
                    {
                        param_chr[0]  = (char) va_arg(param, int);
                        str_to_output = param_chr;
                        break;
                    }

                default:
                    {
                        param_chr[0]  = fmt_str[curr_char];
                        str_to_output = param_chr;
                    }
            } /* switch (fmt_str[curr_char])      */
            curr_char++;
        }     /* if (fmt_str[curr_char++] == '%') */
        my_puts(str_to_output);
    }         /* while (fmt_str[curr_char])       */

    va_end(param);
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
