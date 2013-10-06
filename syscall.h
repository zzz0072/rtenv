#ifndef SYSCALL_H_20130919
#define SYSCALL_H_20130919
#include <stddef.h>
#include <syscall_def.h>
extern void *memcpy(void *dest, const void *src, size_t n);
/* Prototypes */
void *activate(void *stack);

int fork(const void *proc_descption);
int getpid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void interrupt_wait(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);

void sleep(unsigned int);

int set_task_name(const void *proc_name, size_t count);
int get_task_name(void *proc_name, size_t count);

#ifdef USE_SEMIHOST
int host_call(enum HOST_SYSCALL action, void *params);
int host_open(const char *pathname, int flags);
size_t host_read(int fd, void *buf, size_t count);
size_t host_write(int fd, const void *buf, size_t count);
int host_close(int fd);
#endif

#endif /* SYSCALL_H_20130919 */
