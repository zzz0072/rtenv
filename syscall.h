#ifndef SYSCALL_H_20130919
#define SYSCALL_H_20130919
#include <stddef.h>

/* System calls */
#define SYS_CALL_FORK        (0x01)
#define SYS_CALL_GETPID      (0x02)
#define SYS_CALL_WRITE       (0x03)
#define SYS_CALL_READ        (0x04)
#define SYS_CALL_WAIT_INTR   (0x05)
#define SYS_CALL_GETPRIORITY (0x06)
#define SYS_CALL_SETPRIORITY (0x07)
#define SYS_CALL_MK_NODE     (0x08)
#define SYS_CALL_SLEEP       (0x09)

/* Prototypes */
void *activate(void *stack);

int fork();
int getpid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void interrupt_wait(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);

void sleep(unsigned int);

#endif /* SYSCALL_H_20130919 */
