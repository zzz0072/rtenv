#ifndef SYSCALL_DEF_H_20130919
#define SYSCALL_DEF_H_20130919

/* System calls */
#define SYS_CALL_FORK          (0x01)
#define SYS_CALL_GETPID        (0x02)
#define SYS_CALL_WRITE         (0x03)
#define SYS_CALL_READ          (0x04)
#define SYS_CALL_WAIT_INTR     (0x05)
#define SYS_CALL_GETPRIORITY   (0x06)
#define SYS_CALL_SETPRIORITY   (0x07)
#define SYS_CALL_MK_NODE       (0x08)
#define SYS_CALL_SLEEP         (0x09)
#define SYS_CALL_GET_PROC_NAME (0x0a)
#define SYS_CALL_SET_PROC_NAME (0x0b)

#ifdef USE_SEMIHOST
/* Refer to SYS_OPEN ARM document */
enum File_Type_t {
    OPEN_RD = 0,
    OPEN_RD_BIN, 
    OPEN_RD_ONLY,
    OPEN_RD_ONLY_BIN,
    OPEN_WR,
    OPEN_WR_BIN,
    OPEN_WR_ONLY,
    OPEN_WR_ONLY_BIN,
    OPEN_APPEND,
    OPEN_APPEND_BIN,
    OPEN_APPEND_ONLY,
    OPEN_APPEND_ONLY_BIN
};

/* Referred to:
 * ARM documents */
enum HOST_SYSCALL {
    HOSTCALL_OPEN        = 0x01,
    HOSTCALL_CLOSE       = 0x02,
    HOSTCALL_WRITEC      = 0x03,
    HOSTCALL_WRITE0      = 0x04,
    HOSTCALL_WRITE       = 0x05,
    HOSTCALL_READ        = 0x06,
    HOSTCALL_READC       = 0x07,
    HOSTCALL_ISERROR     = 0x08,
    HOSTCALL_ISTTY       = 0x09,
    HOSTCALL_SEEK        = 0x0A,
    HOSTCALL_FLEN        = 0x0C,
    HOSTCALL_REMOVE      = 0x0E,
    HOSTCALL_TMPNAM      = 0x0D,
    HOSTCALL_RENAME      = 0x0F,
    HOSTCALL_CLOCK       = 0x10,
    HOSTCALL_TIME        = 0x11,
    HOSTCALL_HOSTTEM     = 0x12,
    HOSTCALL_ERRNO       = 0x13,
    HOSTCALL_GET_CMDLINE = 0x15,
    HOSTCALL_HEAPINFO    = 0x16,
    HOSTCALL_ELAPSED     = 0x30,
    HOSTCALL_TICKFREQ    = 0x31
};
#endif /* USE_SEMIHOST */
#endif /* SYSCALL_DEF_H_20130919 */
