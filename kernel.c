#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"
#include "syscall.h"
#include "str_util.h"
#include "task.h"
#include "path_server.h"
#include "serial.h"
#include "shell.h"
#include "common_define.h"

/* TODO: Remove global varialbe */
struct task_info g_task_info;

void init_user_tasks()
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

    if (!fork("shell_task")) {
        setpriority(0, PRIORITY_DEFAULT - 10);
        shell_task();
    }

    setpriority(0, PRIORITY_LIMIT);

    while(1);
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
    int empty_task;
    struct task_control_block *task;
    int timeup;
    unsigned int tick_count = 0;

    SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

    init_rs232();
    __enable_irq();

    tasks[task_count].stack = (void*)init_task(stacks[task_count], &init_user_tasks);
    tasks[task_count].tid = 0;
    tasks[task_count].priority = PRIORITY_DEFAULT;

    copy_task_name((void *)tasks[task_count].name, (void *)"Init", 5);
    task_count++;

    /* dirty global tasks */
    g_task_info.tasks = tasks;
    g_task_info.task_amount = &task_count;

    /* Initialize all pipes */
    for (i = 1; i < TASK_LIMIT; i++)
        tasks[i].status = TASK_IS_EMPTY;

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
            empty_task = find_next_empty_task_slot(tasks);
            if (empty_task == TASK_LIMIT) {
                /* Cannot create a new task, return error */
                tasks[current_task].stack->r0 = -1;
            }
            else {
                /* Compute how much of the stack is used */
                size_t used = stacks[current_task] + STACK_SIZE
                          - (unsigned int*)tasks[current_task].stack;

                /* New stack is END - used */
                tasks[empty_task].stack = (void*)(stacks[empty_task] + STACK_SIZE - used);

                /* Copy only the used part of the stack */
                memcpy(tasks[empty_task].stack, tasks[current_task].stack,
                       used * sizeof(unsigned int));

                /* Set TID */
                tasks[empty_task].tid = empty_task;

                /* No more empty */
                tasks[empty_task].status = TASK_CREATED;

                /* Set priority, inherited from forked task */
                tasks[empty_task].priority = tasks[current_task].priority;

                /* Set task name */
                copy_task_name((void *)tasks[empty_task].name,
                                (void *)tasks[empty_task].stack->r0,
                                strlen((void *)tasks[empty_task].stack->r0) + 1);

                /* Set return values in each process */
                tasks[current_task].stack->r0 = empty_task;
                tasks[empty_task].stack->r0 = 0;
                tasks[empty_task].prev = NULL;
                tasks[empty_task].next = NULL;
                task_push(&ready_list[tasks[empty_task].priority], &tasks[empty_task]);

                /* There is now one more task */
                task_count++;
            }
            break;

        case SYS_CALL_TASK_EXIT:
            {
                /* Prevent task being pushed into list */
                tasks[current_task].status = TASK_IS_EMPTY;

                /* TODO: Clean up stack */
            }
            break;

        case SYS_CALL_GETTID:
            tasks[current_task].stack->r0 = current_task;
            break;

        case SYS_CALL_WRITE:
            _write(&tasks[current_task], tasks, pipes);
            break;

        case SYS_CALL_READ:
            _read(&tasks[current_task], tasks, pipes);
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

                if (who == 0) /* Special case handle first */
                    tasks[current_task].stack->r0 = tasks[current_task].priority;
                else if (is_task_valid(tasks, who) == RT_YES)
                    tasks[current_task].stack->r0 = tasks[who].priority;
                else
                    tasks[current_task].stack->r0 = -1;
            } break;

        case SYS_CALL_SETPRIORITY:
            {
                int who = tasks[current_task].stack->r0;
                int value = tasks[current_task].stack->r1;
                value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);

                if (who == 0) /* Special case handle first */
                    tasks[current_task].priority = value;
                else if (is_task_valid(tasks, who) == RT_YES)
                    tasks[who].priority = value;
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

        case SYS_CALL_GET_TASK_NAME:
            {
                copy_task_name((void *)tasks[current_task].stack->r0,
                                (void *)tasks[current_task].name,
                                tasks[current_task].stack->r1);
            } break;

        case SYS_CALL_SET_TASK_NAME:
            {
                copy_task_name((void *)tasks[current_task].name,
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
                for (i = 0; i < TASK_LIMIT; i++) {
                    if(tasks[i].status != TASK_IS_EMPTY) {
                        if ((tasks[i].status == TASK_WAIT_INTR && tasks[i].stack->r0 == intr) ||
                            (tasks[i].status == TASK_WAIT_TIME && tasks[i].stack->r0 == tick_count))
                            tasks[i].status = TASK_READY;
                    }
                }
            } /* default */
        }     /* Switch */

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
        else if (tasks[current_task].status != TASK_IS_EMPTY) {
            task_push(&wait_list, &tasks[current_task]);
        }
        while (ready_list[i] == NULL)
            i++;
        current_task = task_pop(&ready_list[i])->tid;
    }

    return 0;
}
