#ifndef TASK_H_20131005
#define TASK_H_20131005

#define MAX_NAME_CHARS (32)
#define STACK_SIZE 512 /* Size of task stacks in words */
#define TASK_LIMIT 8  /* Max number of tasks we can handle */

#define PRIORITY_DEFAULT 20
#define PRIORITY_LIMIT (PRIORITY_DEFAULT * 2 - 1)

#define TASK_READY      0
#define TASK_WAIT_READ  1
#define TASK_WAIT_WRITE 2
#define TASK_WAIT_INTR  3
#define TASK_WAIT_TIME  4

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
    unsigned int *task_amount;
};

char *get_task_status(int status);

struct task_control_block*
    task_pop (struct task_control_block **list);

int task_push (struct task_control_block **list,
                struct task_control_block *item);

unsigned int *init_task(unsigned int *stack, void (*start)());
void copy_task_name(void *dst, void *src, int char_to_copied);

#endif
