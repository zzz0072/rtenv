#include "path_server.h"
#include "serial.h"
#include "syscall.h"
#include "str_util.h"
#include "task.h"
#include "common_define.h"

#define BACKSPACE (127)
#define ESC        (27)
#define SPACE      (32)

extern struct task_info g_task_info;

static void read_line(char *token, int max_token_chars)
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

/* Tokens */
#define MAX_TOKENS (2)
struct tokens_t
{
    int count;
    char token[MAX_TOKENS][MAX_MSG_CHARS];
};
typedef struct tokens_t tokens;

/* This is not strtok. Internal Field Separator is SPACE only */
void str_decompose(char *line, tokens *str_tokens)
{
    int i = 0;
    int curr_pos = 0;
    int repeated_space = 0;
    int is_quoting = 0;

    /* Init */
    str_tokens->count = 0;

    for(i = 0; i < strlen(line); i++) {
        /* End condition always check first */
        if (line[i] == '\n') {
            /* Is it trailing space? */
            if (repeated_space && str_tokens->count > 1) {
                str_tokens->count--;
            }
            else {
                str_tokens->token[str_tokens->count][curr_pos] = 0;
                str_tokens->count++;
            }

            return;
        }

        /* We have new token while token amount is full */
        if (str_tokens->count == MAX_TOKENS && line[i] != SPACE) {
            return;
        }

        /* Bypass beginning space */
        if (i == 0 && line[0] == SPACE) {
            continue;
        }

        /* Bypass repeated SPACE */
        if (line[i - 1] == SPACE && line[i] == SPACE) {
            repeated_space = 1;
            continue;
        }

        /* Deal with quoting */
        if (line[i] == '"' ) {
            is_quoting = !is_quoting;
            continue;
        }

        /* Take char to token */
        str_tokens->token[str_tokens->count][curr_pos++] = line[i];

        /* Seperate token */
        if (line[i] == SPACE && !is_quoting) {
            str_tokens->token[str_tokens->count][curr_pos - 1] = 0;
            str_tokens->count++;
            curr_pos = 0;
            repeated_space = 0;

            /* Avoid case such as 'ps \n' */
            if (line[i + 1] == '\n') {
                return;
            }
        }
    }
}

static void ps_cmd(void)
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

/**************************************/
/* process command                    */
/**************************************/
typedef void (*cmd_func_t)(tokens *cmd);
struct cmd_t
{
    char *name;
    char *desc;
    char token_num;
    cmd_func_t handler;
};

static void t_exit_cmd(tokens *cmd);
static void help_cmd(tokens *cmd);
static void system_cmd(tokens *cmd);

#define CMD(NAME, DESC, TOKEN_NUM) {.name = #NAME, \
                                    .desc = DESC,  \
                                    .token_num = TOKEN_NUM, \
                                    NAME ## _cmd }

typedef struct cmd_t cmd_entry;
static cmd_entry available_cmds[] = {
        CMD(ps,     "List process", 1),
        CMD(help,   "This menu", 1),
        #ifdef USE_SEMIHOST
        CMD(system, "system\n\r\t\tRun host command", 1),
        #endif
        CMD(t_exit, "Test exit", 1)
};

#define CMD_NUM (sizeof(available_cmds)/sizeof(cmd_entry))

#ifdef USE_SEMIHOST
static void system_cmd(tokens *cmd)
{
    char host_cmd[MAX_MSG_CHARS];

    my_printf("\n\rEnter host command: ");
    read_line(host_cmd, MAX_MSG_CHARS);

    if (strlen(host_cmd) < MAX_MSG_CHARS - 1 && host_cmd[0] != '\n') {
        host_system(host_cmd, strlen(host_cmd));
    }
}
#endif
static void exit_test_task()
{
    my_printf("\rtest...sleep 3 second\n");
    sleep(3000); /* ms */
    my_printf("\rAbout to exit\n");
    exit(0);
}

static void t_exit_cmd(tokens *cmd) 
{
    if (!fork("exit_test_task")) {
        setpriority(0, PRIORITY_DEFAULT - 10);
        exit_test_task();
    }
}

static void help_cmd(tokens *cmd)
{
    int i = 0;

    my_printf("\rAvailable Commands:\n");
    for (i = 0; i < CMD_NUM; i++) {
        my_printf("\r%s\t\t%s\n", available_cmds[i].name, available_cmds[i].desc);
    }
}

static void proc_cmd(tokens *cmd)
{
    int i = 0;

    /* Lets process command */
    my_printf("\n");
    for (i = 0; i < CMD_NUM; i++) {
        if (strncmp(cmd->token[0], available_cmds[i].name, strlen(available_cmds[i].name)) == 0) {
            /* Avoid subset case -> valid cmd: "ps" vs user input: "ps1" */
            if (cmd->token[0][strlen(available_cmds[i].name)] != 0 ) {
                continue;
            }

            /* Parameter num should match */
            if (cmd->count != available_cmds[i].token_num) {
                my_printf("\rToken number not match.\n");
                my_printf("\rExpect %d, get %d.\n", available_cmds[i].token_num, cmd->count);

                for (int i = 0; i < cmd->count; i++) {
                    my_printf("\rtoken[%d]:%s\n", i, cmd->token[i]);
                }
                return;
            }

            /* Run command */
            available_cmds[i].handler(cmd);
            return;
        }
    }
    my_printf("\rCommand not found.\n");
}

void shell_task()
{
    char line[MAX_MSG_CHARS];
    tokens cmd_tokens;

    help_cmd(0);
    while (1) {
        /* Show prompt */
        my_printf("\n\r$ ");
        read_line(line, MAX_MSG_CHARS);

        /* Decompose one line to tokens */
        memset((void*)&cmd_tokens, 0x00, sizeof(cmd_tokens));
        str_decompose(line, &cmd_tokens);

        /* Process command */
        if (strlen(cmd_tokens.token[0]) < MAX_MSG_CHARS - 1 && cmd_tokens.token[0] != '\n') {
            proc_cmd(&cmd_tokens);
        }
    }
}
