#include "syscall_def.h"
#include "str_util.h"
#include <stddef.h>

#define ENCLOSE_QUOTE(VAR) #VAR
#define TO_STR(VAR) ENCLOSE_QUOTE(VAR)

#define SYS_CALL_BODY(ACT) \
    __asm__( \
      "push {r7}\n" \
      "mov r7," ACT "\n"\
      "svc 0\n"\
      "nop\n"\
      "pop {r7}\n"\
      "bx lr\n"\
        :::\
    );


int fork(const void *proc_descption) __attribute__ ((naked));
int fork(const void *proc_descption)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_FORK));
}

int getpid() __attribute__ ((naked));
int getpid()
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_GETPID));
}

int write(int fd, const void *buf, size_t count) __attribute__ ((naked));
int write(int fd, const void *buf, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_WRITE));
}

int read(int fd, void *buf, size_t count) __attribute__ ((naked));
int read(int fd, void *buf, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_READ));
}

void interrupt_wait(int intr) __attribute__ ((naked));
void interrupt_wait(int intr)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_WAIT_INTR));
}

int getpriority(int who) __attribute__ ((naked));
int getpriority(int who)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_GETPRIORITY));
}

int setpriority(int who, int value) __attribute__ ((naked));
int setpriority(int who, int value)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SETPRIORITY));
}

int mknod(int fd, int mode, int dev) __attribute__ ((naked));
int mknod(int fd, int mode, int dev)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_MK_NODE));
}

void sleep(unsigned int msec) __attribute__ ((naked));
void sleep(unsigned int msec)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SLEEP));
}

int setProcName(const void *proc_name, size_t count) __attribute__ ((naked));
int setProcName(const void *proc_name, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_GET_PROC_NAME));
}

int getProcName(void *proc_name, size_t count) __attribute__ ((naked));
int getProcName(void *proc_name, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SET_PROC_NAME));
}

#ifdef USE_SEMIHOST
/* Semihost system call parameters */
union param_t
{
    int   pdInt;
    void *pdPtr;
    char *pdChrPtr;
};

typedef union param_t param;
int hostCall(enum HOST_SYSCALL action, void *arg) __attribute__ ((naked));
int hostCall(enum HOST_SYSCALL action, void *arg)
{
    /* For Thumb-2 code use the BKPT instruction instead of SWI.
     * Refer to:
     * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471c/Bgbjhiea.html
     * http://en.wikipedia.org/wiki/ARM_Cortex-M#Cortex-M4 */

    __asm__( \
      "bkpt 0xAB\n"\
      "nop\n" \
      "bx lr\n"\
        :::\
    );
}

/* Detailed parameters please refer to
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471c/Bgbjhiea.html */
int host_open(const char *pathname, int flags)
{
    param semi_param[3] = {0};

    /* Setting param */
    semi_param[0].pdChrPtr = (char *) pathname;
    semi_param[1].pdInt    = flags;
    semi_param[2].pdInt    = strlen(pathname);

    return hostCall(HOSTCALL_OPEN, semi_param);
}

size_t host_read(int fd, void *buf, size_t count)
{
    param semi_param[3] = {0};

    /* Setting param */
    semi_param[0].pdInt = fd;
    semi_param[1].pdPtr = buf;
    semi_param[2].pdInt = count;

    return hostCall(HOSTCALL_READ, semi_param);
}

size_t host_write(int fd, const void *buf, size_t count)
{
    param semi_param[3] = {0};

    /* Setting param */
    semi_param[0].pdInt = fd;
    semi_param[1].pdPtr = (void *) buf;
    semi_param[2].pdInt = count;

    return hostCall(HOSTCALL_WRITE, semi_param);
}

int host_close(int fd)
{
    return hostCall(HOSTCALL_CLOSE, (void *)&fd);
}

#endif /* USE_SEMIHOST */
