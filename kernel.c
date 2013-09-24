#include "stm32f10x.h"
#include "RTOSConfig.h"
#include "syscall.h"
#include "str_util.h"

#include <stddef.h>

#define STACK_SIZE 512 /* Size of task stacks in words */
#define TASK_LIMIT 8  /* Max number of tasks we can handle */
#define PIPE_BUF   64 /* Size of largest atomic pipe message */
#define PATH_MAX   32 /* Longest absolute path */
#define PIPE_LIMIT (TASK_LIMIT * 2)
#define MAX_NAME_CHARS (32)

#define PATHSERVER_FD (TASK_LIMIT + 3) 
/* File descriptor of pipe to pathserver */

#define PRIORITY_DEFAULT 20
#define PRIORITY_LIMIT (PRIORITY_DEFAULT * 2 - 1)

#define TASK_READY      0
#define TASK_WAIT_READ  1
#define TASK_WAIT_WRITE 2
#define TASK_WAIT_INTR  3
#define TASK_WAIT_TIME  4

#define S_IFIFO 1
#define S_IMSGQ 2

#define O_CREAT 4

#define BACK_SPACE (127)

/* Stack struct of user thread, see "Exception entry and return" */
struct user_thread_stack {
    unsigned int r4;
    unsigned int r5;
    unsigned int r6;
    unsigned int r7;
    unsigned int r8;
    unsigned int r9;
    unsigned int r10;
    unsigned int fp;
    unsigned int _lr;    /* Back to system calls or return exception */
    unsigned int _r7;    /* Backup from isr */
    unsigned int r0;
    unsigned int r1;
    unsigned int r2;
    unsigned int r3;
    unsigned int ip;
    unsigned int lr;    /* Back to user thread code */
    unsigned int pc;
    unsigned int xpsr;
    unsigned int stack[STACK_SIZE - 18];
};

/* Task Control Block */
struct task_control_block {
    struct user_thread_stack *stack;
    int pid;
    int status;
    int priority;
    char name[MAX_NAME_CHARS];
    struct task_control_block **prev;
    struct task_control_block  *next;
};

struct task_info {
    struct task_control_block *tasks;
    int *task_amount;
};

static struct task_info g_task_info;

static char *get_task_status(int status)
{
    switch (status) {
        case TASK_READY:
            return "Ready   ";
        case TASK_WAIT_READ:
            return "Wait read";
        case TASK_WAIT_WRITE:
            return "Wait write";
        case TASK_WAIT_INTR:
            return "Wait intr";
        case TASK_WAIT_TIME:
            return "Wait time";
        default:
            return "Unknown Status";
    }
}

/* 
 * pathserver assumes that all files are FIFOs that were registered
 * with mkfifo.  It also assumes a global tables of FDs shared by all
 * processes.  It would have to get much smarter to be generally useful.
 *
 * The first TASK_LIMIT FDs are reserved for use by their respective tasks.
 * 0-2 are reserved FDs and are skipped.
 * The server registers itself at /sys/pathserver
*/
#define PATH_SERVER_NAME "/sys/pathserver"
void pathserver()
{
    char paths[PIPE_LIMIT - TASK_LIMIT - 3][PATH_MAX];
    int npaths = 0;
    int i = 0;
    unsigned int plen = 0;
    unsigned int replyfd = 0;
    char path[PATH_MAX];

    memcpy(paths[npaths++], PATH_SERVER_NAME, sizeof(PATH_SERVER_NAME));

    while (1) {
        read(PATHSERVER_FD, &replyfd, 4);
        read(PATHSERVER_FD, &plen, 4);
        read(PATHSERVER_FD, path, plen);

        if (!replyfd) { /* mkfifo */
            int dev;
            read(PATHSERVER_FD, &dev, 4);
            memcpy(paths[npaths], path, plen);
            mknod(npaths + 3 + TASK_LIMIT, 0, dev);
            npaths++;
        }
        else { /* open */
            /* Search for path */
            for (i = 0; i < npaths; i++) {
                if (*paths[i] && strcmp(path, paths[i]) == 0) {
                    i += 3; /* 0-2 are reserved */
                    i += TASK_LIMIT; /* FDs reserved for tasks */
                    write(replyfd, &i, 4);
                    i = 0;
                    break;
                }
            }

            if (i >= npaths) {
                i = -1; /* Error: not found */
                write(replyfd, &i, 4);
            }
        }
    }
}

int mkfile(const char *pathname, int mode, int dev)
{
    size_t plen = strlen(pathname)+1;
    char buf[4+4+PATH_MAX+4];
    (void) mode;

    *((unsigned int *)buf) = 0;
    *((unsigned int *)(buf + 4)) = plen;
    memcpy(buf + 4 + 4, pathname, plen);
    *((int *)(buf + 4 + 4 + plen)) = dev;
    write(PATHSERVER_FD, buf, 4 + 4 + plen + 4);

    return 0;
}

int mkfifo(const char *pathname, int mode)
{
    mkfile(pathname, mode, S_IFIFO);
    return 0;
}

int open(const char *pathname, int flags)
{
    unsigned int replyfd = getpid() + 3;
    size_t plen = strlen(pathname) + 1;
    unsigned int fd = -1;
    char buf[4 + 4 + PATH_MAX];
    (void) flags;

    *((unsigned int *)buf) = replyfd;
    *((unsigned int *)(buf + 4)) = plen;
    memcpy(buf + 4 + 4, pathname, plen);
    write(PATHSERVER_FD, buf, 4 + 4 + plen);
    read(replyfd, &fd, 4);

    return fd;
}

int mq_open(const char *name, int oflag)
{
    if (oflag & O_CREAT)
        mkfile(name, 0, S_IMSGQ);
    return open(name, 0);
}

void serialout(USART_TypeDef* uart, unsigned int intr)
{
    int fd;
    char c;
    int doread = 1;
    mkfifo("/dev/tty0/out", 0);
    fd = open("/dev/tty0/out", 0);

    while (1) {
        if (doread)
            read(fd, &c, 1);
        doread = 0;
        if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
            USART_SendData(uart, c);
            USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
            doread = 1;
        }
        interrupt_wait(intr);
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

void serialin(USART_TypeDef* uart, unsigned int intr)
{
    int fd;
    char c;
    mkfifo("/dev/tty0/in", 0);
    fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    while (1) {
        interrupt_wait(intr);
        if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
            c = USART_ReceiveData(uart);
            write(fd, &c, 1);
        }
    }
}

void greeting()
{
    int fdout = open("/dev/tty0/out", 0);
    char *string = "Hello, World!\n";
    while (*string) {
        write(fdout, string, 1);
        string++;
    }
}

void echo()
{
    int fdout, fdin;
    char c;
    fdout = open("/dev/tty0/out", 0);
    fdin = open("/dev/tty0/in", 0);

    while (1) {
        read(fdin, &c, 1);
        write(fdout, &c, 1);
    }
}

void rs232_xmit_msg_task()
{
    int fdout, fdin;
    char str[100];
    int curr_char;
    fdout = open("/dev/tty0/out", 0);
    fdin = mq_open("/tmp/mqueue/out", O_CREAT);
    setpriority(0, PRIORITY_DEFAULT - 2);

    while (1) {
        /* Read from the queue.  Keep trying until a message is
         * received.  This will block for a period of time (specified
         * by portMAX_DELAY). */
        read(fdin, str, 100);

        /* Write each character of the message to the RS232 port. */
        curr_char = 0;
        while (str[curr_char] != '\0') {
            write(fdout, &str[curr_char], 1);
            curr_char++;
        }
    }
}

void queue_str_task(const char *str, int delay)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);
    int msg_len = strlen(str) + 1;

    while (1) {
        /* Post the message.  Keep on trying until it is successful. */
        write(fdout, str, msg_len);

        /* Wait. */
        sleep(delay);
    }
}

void queue_str_task1()
{
    queue_str_task("Hello 1\n", 200);
}

void queue_str_task2()
{
    queue_str_task("Hello 2\n", 50);
}

static void cmd_ps(void)
{
    int i = 0;
    char msg[MAX_MSG_CHARS];

    my_print("\rList process\n");

    /* This should not happen actually */
    if (!g_task_info.tasks) {
        return;
    }

    /* Start list */
    for (i = 0; i < *(g_task_info.task_amount); i++) {
        /* PID */
        my_print("\rPID: ");
        my_print(itoa(g_task_info.tasks[i].pid));

        /* Priority */
        my_print("\tPriority: ");
        my_print(itoa(g_task_info.tasks[i].priority));

        /* Status */
        my_print("\tStatus: ");
        my_print(get_task_status(g_task_info.tasks[i].status));

        /* Process name */
        my_print("\t");
        my_print(g_task_info.tasks[i].name);

        my_print("\n");
    }
}

typedef void (*cmd_func_t)(void);
struct cmd_t
{
    char *name;
    char *desc;
    cmd_func_t handler;
};

static void help_menu(void);
typedef struct cmd_t cmd_entry;
static cmd_entry available_cmds[] = {
        {
            .name = "ps",
            .desc = "List process",
            .handler = cmd_ps
        },
        {
            .name = "help",
            .desc = "This menu",
            .handler = help_menu
        }
};

static void help_menu(void)
{
    int i = 0;

    my_print("\rAvailable Commands:\n");
    for (i = 0; i < sizeof(available_cmds)/sizeof(cmd_entry); i++) {
        my_print("\r");
        my_print(available_cmds[i].name);
        my_print("\t\t");
        my_print(available_cmds[i].desc);
        my_print("\n");
    }
}

static void proc_cmd(int out_fd, char *cmd, int cmd_char_num)
{
    int i = 0;

    /* Lets process command */
    for (i = 0; i < sizeof(available_cmds)/sizeof(cmd_entry); i++) {
        if (strncmp(cmd, available_cmds[i].name, strlen(available_cmds[i].name)) == 0) {
            /* Avoid subset case -> valid cmd: "ps" vs user input: "ps1" */
            if (cmd[strlen(available_cmds[i].name)] != '\n' ) {
                continue;
            }
            available_cmds[i].handler();
            return;
        }
    }
    my_print("\rCommand not found.\n");
}

void serial_readwrite_task()
{
    int fdout, fdin;
    char str[MAX_MSG_CHARS];
    char ch[] = {0x00, 0x00};
    int curr_char;

    fdout = mq_open("/tmp/mqueue/out", 0);
    fdin = open("/dev/tty0/in", 0);

    help_menu();
    while (1) {
        /* Show prompt */
        my_print("\n\r$ ");

        curr_char = 0;
        while(1) {
            /* Receive a byte from the RS232 port (this call will
             * block). */
            read(fdin, &ch[0], 1);

            /* If the byte is an end-of-line type character, then
             * finish the string and inidcate we are done.
             */
            if (curr_char >= 98 || (ch[0] == '\r') || (ch[0] == '\n')) {
                str[curr_char] = '\n';
                str[curr_char+1] = '\0';
                break;
            }
            else if(ch[0] == BACK_SPACE) { /* backspace */
                if(curr_char > 0) {
                    curr_char--;
                    write(fdout, "\b", 1);
                    write(fdout, " ", 1);
                    write(fdout, "\b", 1);
                }
            }
            else {
                /* Appends only when buffer is not full.
               * Include \n\0*/
                if (curr_char < (MAX_MSG_CHARS - 3)) {
                    str[curr_char++] = ch[0];
                    write(fdout, ch, 2);
                }
            }
        }

        /* Once we are done building the response string, queue the
         * response to be sent to the RS232 port.
         */
        my_print("\n");
        if (strlen(str) < MAX_MSG_CHARS - 1 && str[0] != '\n') {
            proc_cmd(fdout, str, curr_char + 1 + 1);
        }
    }
}

void first()
{
    setpriority(0, 0);

    if (!fork("pathserver")) {
        setpriority(0, 0);
        pathserver();
    }

    if (!fork("serialout")) {
        setpriority(0, 0);
        serialout(USART2, USART2_IRQn);
    }

    if (!fork("serialin")) {
        setpriority(0, 0);
        serialin(USART2, USART2_IRQn);
    }

    if (!fork("rs232_xmit_msg_task")) {
        rs232_xmit_msg_task();
    }

    if (!fork("serial_readwrite_task")) {
        setpriority(0, PRIORITY_DEFAULT - 10);
        serial_readwrite_task();
    }

    setpriority(0, PRIORITY_LIMIT);

    while(1);
}

struct pipe_ringbuffer {
    int start;
    int end;
    char data[PIPE_BUF];

    int (*readable) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*writable) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*read) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*write) (struct pipe_ringbuffer*, struct task_control_block*);
};

#define RB_PUSH(rb, size, v) do { \
        (rb).data[(rb).end] = (v); \
        (rb).end++; \
        if ((rb).end >= size) (rb).end = 0; \
    } while (0)

#define RB_POP(rb, size, v) do { \
        (v) = (rb).data[(rb).start]; \
        (rb).start++; \
        if ((rb).start >= size) (rb).start = 0; \
    } while (0)

#define RB_PEEK(rb, size, v, i) do { \
        int _counter = (i); \
        int _src_index = (rb).start; \
        int _dst_index = 0; \
        while (_counter--) { \
            ((char*)&(v))[_dst_index++] = (rb).data[_src_index++]; \
            if (_src_index >= size) _src_index = 0; \
        } \
    } while (0)

#define RB_LEN(rb, size) (((rb).end - (rb).start) + \
    (((rb).end < (rb).start) ? size : 0))

#define PIPE_PUSH(pipe, v) RB_PUSH((pipe), PIPE_BUF, (v))
#define PIPE_POP(pipe, v)  RB_POP((pipe), PIPE_BUF, (v))
#define PIPE_PEEK(pipe, v, i)  RB_PEEK((pipe), PIPE_BUF, (v), (i))
#define PIPE_LEN(pipe)     (RB_LEN((pipe), PIPE_BUF))

unsigned int *init_task(unsigned int *stack, void (*start)())
{
    stack += STACK_SIZE - 9; /* End of stack, minus what we're about to push */
    stack[8] = (unsigned int)start;
    return stack;
}

int
task_push (struct task_control_block **list, struct task_control_block *item)
{
    if (list && item) {
        /* Remove itself from original list */
        if (item->prev)
            *(item->prev) = item->next;
        if (item->next)
            item->next->prev = item->prev;
 
        /* Insert into new list */
        while (*list) list = &((*list)->next);
        *list = item;
        item->prev = list;
        item->next = NULL;
        return 0;
    }
    return -1;
}

struct task_control_block*
task_pop (struct task_control_block **list)
{
    if (list) {
        struct task_control_block *item = *list;
        if (item) {
            *list = item->next;
            if (item->next)
                item->next->prev = list;
            item->prev = NULL;
            item->next = NULL;
            return item;
        }
    }
    return NULL;
}

void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);

void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes)
{
    task->status = TASK_READY;

    /* If the fd is invalid */
    if (task->stack->r0 > PIPE_LIMIT) {
        task->stack->r0 = -1;
    }
    else {
        struct pipe_ringbuffer *pipe = &pipes[task->stack->r0];

        if (pipe->readable(pipe, task)) {
            size_t i;

            pipe->read(pipe, task);

            /* Unblock any waiting writes */
            for (i = 0; i < task_count; i++)
                if (tasks[i].status == TASK_WAIT_WRITE)
                    _write(&tasks[i], tasks, task_count, pipes);
        }
    }
}

static void _copyProcName(void *dst, void *src, int char_to_copied)
{
    int i = 0;

    /* Boundary check */
    if (char_to_copied > MAX_NAME_CHARS) {
        char_to_copied = MAX_NAME_CHARS;
    }

    /* Let's copy */
    memcpy(dst, src, char_to_copied);

    /* Just make sure */
    *((char *)dst + char_to_copied - 1) = 0;
}

void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes)
{
    task->status = TASK_READY;

    /* If the fd is invalid */
    if (task->stack->r0 > PIPE_LIMIT) {
        task->stack->r0 = -1;
    }
    else {
        struct pipe_ringbuffer *pipe = &pipes[task->stack->r0];

        if (pipe->writable(pipe, task)) {
            size_t i;

            pipe->write(pipe, task);

            /* Unblock any waiting reads */
            for (i = 0; i < task_count; i++)
                if (tasks[i].status == TASK_WAIT_READ)
                    _read(&tasks[i], tasks, task_count, pipes);
        }
    }
}

int
fifo_readable (struct pipe_ringbuffer *pipe,
               struct task_control_block *task)
{
    /* Trying to read too much */
    if (task->stack->r2 > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }
    if ((size_t)PIPE_LEN(*pipe) < task->stack->r2) {
        /* Trying to read more than there is: block */
        task->status = TASK_WAIT_READ;
        return 0;
    }
    return 1;
}

int
mq_readable (struct pipe_ringbuffer *pipe,
             struct task_control_block *task)
{
    size_t msg_len;

    /* Trying to read too much */
    if ((size_t)PIPE_LEN(*pipe) < sizeof(size_t)) {
        /* Nothing to read */
        task->status = TASK_WAIT_READ;
        return 0;
    }

    PIPE_PEEK(*pipe, msg_len, 4);

    if (msg_len > task->stack->r2) {
        /* Trying to read more than buffer size */
        task->stack->r0 = -1;
        return 0;
    }
    return 1;
}

int
fifo_read (struct pipe_ringbuffer *pipe,
           struct task_control_block *task)
{
    size_t i;
    char *buf = (char*)task->stack->r1;

    /* Copy data into buf */
    for (i = 0; i < task->stack->r2; i++) {
        PIPE_POP(*pipe, buf[i]);
    }
    return task->stack->r2;
}

int
mq_read (struct pipe_ringbuffer *pipe,
         struct task_control_block *task)
{
    size_t msg_len;
    size_t i;
    char *buf = (char*)task->stack->r1;
    /* Get length */
    for (i = 0; i < 4; i++) {
        PIPE_POP(*pipe, *(((char*)&msg_len)+i));
    }
    /* Copy data into buf */
    for (i = 0; i < msg_len; i++) {
        PIPE_POP(*pipe, buf[i]);
    }
    return msg_len;
}

int
fifo_writable (struct pipe_ringbuffer *pipe,
               struct task_control_block *task)
{
    /* If the write would be non-atomic */
    if (task->stack->r2 > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }
 
    /* Preserve 1 byte to distiguish empty or full */
    if ((size_t)PIPE_BUF - PIPE_LEN(*pipe) - 1 < task->stack->r2) {
        /* Trying to write more than we have space for: block */
        task->status = TASK_WAIT_WRITE;
        return 0;
    }
    return 1;
}

int
mq_writable (struct pipe_ringbuffer *pipe,
             struct task_control_block *task)
{
    size_t total_len = sizeof(size_t) + task->stack->r2;

    /* If the write would be non-atomic */
    if (total_len > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }

    /* Preserve 1 byte to distiguish empty or full */
    if ((size_t)PIPE_BUF - PIPE_LEN(*pipe) - 1 < total_len) {
        /* Trying to write more than we have space for: block */
        task->status = TASK_WAIT_WRITE;
        return 0;
    }
    return 1;
}

int
fifo_write (struct pipe_ringbuffer *pipe,
            struct task_control_block *task)
{
    size_t i;
    const char *buf = (const char*)task->stack->r1;

    /* Copy data into pipe */
    for (i = 0; i < task->stack->r2; i++)
        PIPE_PUSH(*pipe,buf[i]);
    return task->stack->r2;
}

int
mq_write (struct pipe_ringbuffer *pipe,
          struct task_control_block *task)
{
    size_t i;
    const char *buf = (const char*)task->stack->r1;

    /* Copy count into pipe */
    for (i = 0; i < sizeof(size_t); i++)
        PIPE_PUSH(*pipe, *(((char*)&task->stack->r2) + i));

    /* Copy data into pipe */
    for (i = 0; i < task->stack->r2; i++)
        PIPE_PUSH(*pipe, buf[i]);
    return task->stack->r2;
}

int
_mknod(struct pipe_ringbuffer *pipe, int dev)
{
    switch(dev) {
    case S_IFIFO:
        pipe->readable = fifo_readable;
        pipe->writable = fifo_writable;
        pipe->read = fifo_read;
        pipe->write = fifo_write;
        break;
    case S_IMSGQ:
        pipe->readable = mq_readable;
        pipe->writable = mq_writable;
        pipe->read = mq_read;
        pipe->write = mq_write;
        break;
    default:
        return 1;
    }
    return 0;
}

int main()
{
    unsigned int stacks[TASK_LIMIT][STACK_SIZE];
    struct task_control_block tasks[TASK_LIMIT];
    struct pipe_ringbuffer pipes[PIPE_LIMIT];
    struct task_control_block *ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
    struct task_control_block *wait_list = NULL;
    size_t task_count = 0;
    size_t current_task = 0;
    size_t i;
    struct task_control_block *task;
    int timeup;
    unsigned int tick_count = 0;

    SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

    init_rs232();
    __enable_irq();

    tasks[task_count].stack = (void*)init_task(stacks[task_count], &first);
    tasks[task_count].pid = 0;
    tasks[task_count].priority = PRIORITY_DEFAULT;
    
    _copyProcName((void *)tasks[task_count].name,
                    (void *)"Init", 5);
    task_count++;
    
    /* dirty global tasks */
    g_task_info.tasks = tasks;
    g_task_info.task_amount = &task_count;

    /* Initialize all pipes */
    for (i = 0; i < PIPE_LIMIT; i++)
        pipes[i].start = pipes[i].end = 0;

    /* Initialize fifos */
    for (i = 0; i <= PATHSERVER_FD; i++)
        _mknod(&pipes[i], S_IFIFO);

    /* Initialize ready lists */
    for (i = 0; i <= PRIORITY_LIMIT; i++)
        ready_list[i] = NULL;

    while (1) {
        tasks[current_task].stack = activate(tasks[current_task].stack);
        tasks[current_task].status = TASK_READY;
        timeup = 0;

        switch (tasks[current_task].stack->r7) {
        case SYS_CALL_FORK:
            if (task_count == TASK_LIMIT) {
                /* Cannot create a new task, return error */
                tasks[current_task].stack->r0 = -1;
            }
            else {
                /* Compute how much of the stack is used */
                size_t used = stacks[current_task] + STACK_SIZE
                          - (unsigned int*)tasks[current_task].stack;

                /* New stack is END - used */
                tasks[task_count].stack = (void*)(stacks[task_count] + STACK_SIZE - used);

                /* Copy only the used part of the stack */
                memcpy(tasks[task_count].stack, tasks[current_task].stack,
                       used * sizeof(unsigned int));

                /* Set PID */
                tasks[task_count].pid = task_count;

                /* Set priority, inherited from forked task */
                tasks[task_count].priority = tasks[current_task].priority;

                /* Set process name */
                _copyProcName((void *)tasks[task_count].name,
                                (void *)tasks[task_count].stack->r0,
                                strlen((void *)tasks[task_count].stack->r0) + 1);

                /* Set return values in each process */
                tasks[current_task].stack->r0 = task_count;
                tasks[task_count].stack->r0 = 0;
                tasks[task_count].prev = NULL;
                tasks[task_count].next = NULL;
                task_push(&ready_list[tasks[task_count].priority], &tasks[task_count]);

                /* There is now one more task */
                task_count++;
            }
            break;

        case SYS_CALL_GETPID:
            tasks[current_task].stack->r0 = current_task;
            break;

        case SYS_CALL_WRITE:
            _write(&tasks[current_task], tasks, task_count, pipes);
            break;

        case SYS_CALL_READ:
            _read(&tasks[current_task], tasks, task_count, pipes);
            break;

        case SYS_CALL_WAIT_INTR:
            /* Enable interrupt */
            NVIC_EnableIRQ(tasks[current_task].stack->r0);

            /* Block task waiting for interrupt to happen */
            tasks[current_task].status = TASK_WAIT_INTR;
            break;

        case SYS_CALL_GETPRIORITY:
            {
                int who = tasks[current_task].stack->r0;
                if (who > 0 && who < (int)task_count)
                    tasks[current_task].stack->r0 = tasks[who].priority;
                else if (who == 0)
                    tasks[current_task].stack->r0 = tasks[current_task].priority;
                else
                    tasks[current_task].stack->r0 = -1;
            } break;

        case SYS_CALL_SETPRIORITY:
            {
                int who = tasks[current_task].stack->r0;
                int value = tasks[current_task].stack->r1;
                value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
                if (who > 0 && who < (int)task_count)
                    tasks[who].priority = value;
                else if (who == 0)
                    tasks[current_task].priority = value;
                else {
                    tasks[current_task].stack->r0 = -1;
                    break;
                }
                tasks[current_task].stack->r0 = 0;
            } break;

        case SYS_CALL_MK_NODE:
            if (tasks[current_task].stack->r0 < PIPE_LIMIT)
                tasks[current_task].stack->r0 =
                    _mknod(&pipes[tasks[current_task].stack->r0],
                           tasks[current_task].stack->r2);
            else
                tasks[current_task].stack->r0 = -1;
            break;

        case SYS_CALL_SLEEP:
            if (tasks[current_task].stack->r0 != 0) {
                tasks[current_task].stack->r0 += tick_count;
                tasks[current_task].status = TASK_WAIT_TIME;
            }
            break;

        case SYS_CALL_GET_PROC_NAME:
            {
                _copyProcName((void *)tasks[current_task].stack->r0,
                                (void *)tasks[current_task].name,
                                tasks[current_task].stack->r1);
            } break;

        case SYS_CALL_SET_PROC_NAME:
            {
                _copyProcName((void *)tasks[current_task].name,
                                (void *)tasks[current_task].stack->r0,
                                tasks[current_task].stack->r1);
            } break;

        default: /* Catch all interrupts */
            if ((int)tasks[current_task].stack->r7 < 0) {
                unsigned int intr = -tasks[current_task].stack->r7 - 16;

                if (intr == SysTick_IRQn) {
                    /* Never disable timer. We need it for pre-emption */
                    timeup = 1;
                    tick_count++;
                }
                else {
                    /* Disable interrupt, interrupt_wait re-enables */
                    NVIC_DisableIRQ(intr);
                }

                /* Unblock any waiting tasks */
                for (i = 0; i < task_count; i++)
                    if ((tasks[i].status == TASK_WAIT_INTR && tasks[i].stack->r0 == intr) ||
                        (tasks[i].status == TASK_WAIT_TIME && tasks[i].stack->r0 == tick_count))
                        tasks[i].status = TASK_READY;
            }
        }

        /* Put waken tasks in ready list */
        for (task = wait_list; task != NULL;) {
            struct task_control_block *next = task->next;
            if (task->status == TASK_READY)
                task_push(&ready_list[task->priority], task);
            task = next;
        }

        /* Select next TASK_READY task */
        for (i = 0; i < (size_t)tasks[current_task].priority && ready_list[i] == NULL; i++);
        if (tasks[current_task].status == TASK_READY) {
            if (!timeup && i == (size_t)tasks[current_task].priority)
                /* Current task has highest priority and remains execution time */
                continue;
            else
                task_push(&ready_list[tasks[current_task].priority], &tasks[current_task]);
        }
        else {
            task_push(&wait_list, &tasks[current_task]);
        }
        while (ready_list[i] == NULL)
            i++;
        current_task = task_pop(&ready_list[i])->pid;
    }

    return 0;
}
