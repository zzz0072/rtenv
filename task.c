#include "task.h"
#include "str_util.h"

char *get_task_status(int status)
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

void _copyProcName(void *dst, void *src, int char_to_copied)
{
    /* Boundary check */
    if (char_to_copied > MAX_NAME_CHARS) {
        char_to_copied = MAX_NAME_CHARS;
    }

    /* Let's copy */
    memcpy(dst, src, char_to_copied);

    /* Just make sure */
    *((char *)dst + char_to_copied - 1) = 0;
}
