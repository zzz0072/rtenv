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

int intToString(int num, char *str_num, int str_buf_bytes)
{
    char* curr_str_pos = str_num;
    int   digits;

    /* Regular check */
    if (!str_num) {
        return RT_ERR;
    }

    /* Special case: 0 */
    if (!num) {
        *curr_str_pos       = '0';
        *(curr_str_pos + 1) = 0;
        return RT_OK;
    }

    /* Negative number case */
    if (num < 0) {
        *curr_str_pos = '-';
        curr_str_pos++;
        num = -1 * num;
    }
    digits = num;

    /* How any digits we need? */
    while (digits) {
        digits = digits / 10;
        curr_str_pos++;

        /* Buffer full? */
        if ((int)(curr_str_pos - str_num) >=  str_buf_bytes) {
            return RT_ERR;
        }
    }

    /* Convert digit by digit */
    *curr_str_pos = 0;
    while (num) {
        curr_str_pos--;
        *curr_str_pos = (num % 10) + 0x30;
        num = num / 10;
    }

    return RT_OK;
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
#else 
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
