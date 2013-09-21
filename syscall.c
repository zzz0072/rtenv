#include "syscall_def.h"
#include <stddef.h>

int fork(const void *proc_descption) __attribute__ ((naked));
int fork(const void *proc_descption)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x01\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int getpid() __attribute__ ((naked));
int getpid()
{
    asm(
      "push {r7}\n"
      "mov r7, #0x02\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int write(int fd, const void *buf, size_t count) __attribute__ ((naked));
int write(int fd, const void *buf, size_t count)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x03\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int read(int fd, void *buf, size_t count) __attribute__ ((naked));
int read(int fd, void *buf, size_t count)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x04\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

void interrupt_wait(int intr) __attribute__ ((naked));
void interrupt_wait(int intr)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x05\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int getpriority(int who) __attribute__ ((naked));
int getpriority(int who)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x06\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int setpriority(int who, int value) __attribute__ ((naked));
int setpriority(int who, int value)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x07\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int mknod(int fd, int mode, int dev) __attribute__ ((naked));
int mknod(int fd, int mode, int dev)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x08\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

void sleep(unsigned int msec) __attribute__ ((naked));
void sleep(unsigned int msec)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x09\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int setProcName(const void *proc_name, size_t count) __attribute__ ((naked));
int setProcName(const void *proc_name, size_t count)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x0a\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}

int getProcName(void *proc_name, size_t count) __attribute__ ((naked));
int getProcName(void *proc_name, size_t count)
{
    asm(
      "push {r7}\n"
      "mov r7, #0x0b\n"
      "svc 0\n"
      "nop\n"
      "pop {r7}\n"
      "bx lr\n"
        :::
    );
}
