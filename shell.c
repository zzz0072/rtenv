#include "path_server.h"
#include "serial.h"
#include "syscall.h"
#include "str_util.h"
#include "task.h"
#include "common_define.h"

#define BACKSPACE (127)
#define ESC        (27)

extern struct task_info g_task_info;


static void read_token(char *token, int max_token_chars)
{
    int fdin;
    char ch[] = {0x00, 0x00};
    char last_char_is_ESC = RT_NO;
    int curr_char;

    fdin = open("/dev/tty0/in", 0);
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
        if (curr_char == (max_token_chars - 2) || \
            (ch[0] == '\r') || (ch[0] == '\n')) {
            *(token + curr_char) = '\n';
            *(token + curr_char + 1) = '\0';
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
             * Include \n\0 */
            if (curr_char < (max_token_chars - 3)) {
                *(token + curr_char++) = ch[0];
                my_puts(ch);
            }
        }
    }
}

static void cmd_ps(void)
{
    int i = 0;

    my_printf("\rList process\n");

    /* This should not happen actually */
    if (!g_task_info.tasks) {
        return;
    }

    /* Start list */
    for (i = 0; i < TASK_LIMIT; i++) {
        if(g_task_info.tasks[i].status != TASK_IS_EMPTY) {
            my_printf("\rTID: %d\tPriority: %d\tStatus: %s\t%s\n",
            g_task_info.tasks[i].tid,
            g_task_info.tasks[i].priority,
            get_task_status(g_task_info.tasks[i].status),
            g_task_info.tasks[i].name);
        }
    }
}

typedef void (*cmd_func_t)(void);
struct cmd_t
{
    char *name;
    char *desc;
    cmd_func_t handler;
};

static void test_exit(void);
static void help_menu(void);
static void system(void);

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
        },
        #ifdef USE_SEMIHOST
        {
            .name = "system",
            .desc = "system\n\r\t\tRun host command",
            .handler = system
        },
        #endif
        {
            .name = "t_exit",
            .desc = "Test exit",
            .handler = test_exit
        }
};
#ifdef USE_SEMIHOST
static void system(void)
{
    char host_cmd[MAX_MSG_CHARS];

    my_printf("\n\rEnter host command: ");
    read_token(host_cmd, MAX_MSG_CHARS);

    if (strlen(host_cmd) < MAX_MSG_CHARS - 1 && host_cmd[0] != '\n') {
        host_system(host_cmd, strlen(host_cmd));
    }
}
#endif
static void exit_test_task(void)
{
    my_printf("\rtest...sleep 3 second\n");
    sleep(3000); /* ms */
    my_printf("\rAbout to exit\n");
    exit(0);
}

static void test_exit(void) 
{
    if (!fork("exit_test_task")) {
        setpriority(0, PRIORITY_DEFAULT - 10);
        exit_test_task();
    }
}

static void help_menu(void)
{
    int i = 0;

    my_printf("\rAvailable Commands:\n");
    for (i = 0; i < sizeof(available_cmds)/sizeof(cmd_entry); i++) {
        my_printf("\r%s\t\t%s\n", available_cmds[i].name, available_cmds[i].desc);
    }
}

static void proc_cmd(char *cmd)
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
    char str[MAX_MSG_CHARS];

    help_menu();
    while (1) {
        /* Show prompt */
        my_printf("\n\r$ ");
        read_token(str, MAX_MSG_CHARS);

        /* Process command */
        if (strlen(str) < MAX_MSG_CHARS - 1 && str[0] != '\n') {
            proc_cmd(str);
        }
    }
}
