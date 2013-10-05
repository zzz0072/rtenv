#include "path_server.h"
#include "serial.h"
#include "syscall.h"
#include "str_util.h"
#include "task.h"

#define BACKSPACE (127)
#define ESC        (27)

#define RT_NO  (0)
#define RT_YES (1)

extern struct task_info g_task_info;

static void cmd_ps(void)
{
    int i = 0;

    my_printf("\rList process\n");

    /* This should not happen actually */
    if (!g_task_info.tasks) {
        return;
    }

    /* Start list */
    for (i = 0; i < *(g_task_info.task_amount); i++) {
        my_printf("\rPID: %d\tPriority: %d\tStatus: %s\t%s\n",
                    g_task_info.tasks[i].pid,
                    g_task_info.tasks[i].priority,
                    get_task_status(g_task_info.tasks[i].status),
                    g_task_info.tasks[i].name);
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

    my_printf("\rAvailable Commands:\n");
    for (i = 0; i < sizeof(available_cmds)/sizeof(cmd_entry); i++) {
        my_printf("\r%s\t\t%s\n", available_cmds[i].name, available_cmds[i].desc);
    }
}

static void proc_cmd(int out_fd, char *cmd, int cmd_char_num)
{
    int i = 0;

    /* Lets process command */
    my_printf("\n");
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
    my_printf("\rCommand not found.\n");
}

void shell_task()
{
    int fdout, fdin;
    char str[MAX_MSG_CHARS];
    char ch[] = {0x00, 0x00};
    char last_char_is_ESC = RT_NO;
    int curr_char;

    fdout = mq_open("/tmp/mqueue/out", 0);
    fdin = open("/dev/tty0/in", 0);

    help_menu();
    while (1) {
        /* Show prompt */
        my_printf("\n\r$ ");

        curr_char = 0;
        while(1) {
            /* Receive a byte from the RS232 port (this call will
             * block). */
            read(fdin, &ch[0], 1);

            /* Handle ESC case first */
            if (last_char_is_ESC == RT_YES) {
                last_char_is_ESC = RT_NO;

                if (ch[0] == '[') {
                    /* Direction key: ESC[A ~ ESC[D */
                    read(fdin, &ch[0], 1);

                    /* Home:      ESC[1~
                     * End:       ESC[2~
                     * Insert:    ESC[3~
                     * Delete:    ESC[4~
                     * Page up:   ESC[5~
                     * Page down: ESC[6~ */
                    if (ch[0] >= '1' && ch[0] <= '6') {
                        read(fdin, &ch[0], 1);
                    }
                    continue;
                }
            }

            /* If the byte is an end-of-line type character, then
             * finish the string and inidcate we are done.
             */
            if (curr_char >= 98 || (ch[0] == '\r') || (ch[0] == '\n')) {
                str[curr_char] = '\n';
                str[curr_char+1] = '\0';
                break;
            }
            else if(ch[0] == ESC) {
                last_char_is_ESC = RT_YES;
            }
            /* Skip control characters. man ascii for more information */
            else if (ch[0] < 0x20) {
                continue;
            }
            else if(ch[0] == BACKSPACE) { /* backspace */
                if(curr_char > 0) {
                    curr_char--;
                    my_printf("\b \b");
                }
            }
            else {
                /* Appends only when buffer is not full.
               * Include \n\0*/
                if (curr_char < (MAX_MSG_CHARS - 3)) {
                    str[curr_char++] = ch[0];
                    my_puts(ch);
                }
            }
        }

        /* Once we are done building the response string, queue the
         * response to be sent to the RS232 port.
         */
        if (strlen(str) < MAX_MSG_CHARS - 1 && str[0] != '\n') {
            proc_cmd(fdout, str, curr_char + 1 + 1);
        }
    }
}