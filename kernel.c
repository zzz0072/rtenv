#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"
#include "syscall.h"
#include "str_util.h"
#include "task.h"
#include "path_server.h"
#include "serial.h"
#include "shell.h"

/* TODO: Remove global varialbe */
struct task_info g_task_info;

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
