#ifndef SYSCALL_H_20130919
#define SYSCALL_H_20130919
#include <stddef.h>
#include <syscall_def.h>
extern void *memcpy(void *dest, const void *src, size_t n);
/* Prototypes */
int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

int getpriority(int who);
int setpriority(int who, int value);

int set_task_name(const void *proc_name, size_t count);
int get_task_name(void *proc_name, size_t count);

void *activate(void *stack);
void sleep(unsigned int);
void interrupt_wait(int intr);
void exit(int status);
int  fork(const void *proc_descption);
int  gettid();
int  mknod(int fd, int mode, int dev);

#ifdef USE_SEMIHOST
size_t host_read(int fd, void *buf, size_t count);
size_t host_write(int fd, const void *buf, size_t count);

int host_open(const char *pathname, int flags);
int host_close(int fd);
int host_system(char *cmd, int str_len);
#endif

#endif /* SYSCALL_H_20130919 */
