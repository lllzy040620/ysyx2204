/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

#include <memory/vaddr.h> // added for cmd_x

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1); // cpu_exec()接受的参数是uin64_t类型，因此-1意味着一个很大的整数
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args)
{
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int N; // denoted for N in "si [N]"

  if (arg == NULL)
  {
    /* no argument given */
    N = 1;
  }
  else 
  {
    N = atoi(arg);
  }
  
  cpu_exec(N);

  return 0;
}

static int cmd_info(char *args)
{
  /* extract the first argument */
  char *arg = strtok(NULL, " ");

  if (strcmp(arg, "r") == 0)
  {
    isa_reg_display(); // print status of Registers
  }
  else if (strcmp(arg, "w") == 0)
  {
    // TODO: watchpoint
  }
  else
  {
    // exceptions
    printf("Error argument input: r for Registers, w for watch points.\n");
  }

  return 0;
}

static int cmd_x(char *args)
{
  /* extract the arguments */
  char *arg = strtok(NULL, " ");
  char *expr = strtok(NULL, " ");

  // check argument security
  if (arg == NULL || expr == NULL)
  {
    printf("Arguments missing for 'x N EXPR'\n");
  }
  else
  {
    int N = atoi(arg); // N : consecutive N 4-byte memory
    char *expr_end;

    int addr = strtol(expr, &expr_end, 16);

    for (int i = 0; i < N; i++)
    {
      printf("0x%08x:    0x%08x\n", addr, vaddr_read(addr, 4));
      addr += 4;
    }
  }

  return 0;
}

static int cmd_p(char *args)
{
  bool success;
  word_t res = expr(args, &success);

  if (!success)
  {
    puts("invalid expression");
  } 
  else
  {
    printf("%u\n", res);
  }
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute ONE single step instruction", cmd_si },
  { "info", "Print the status of Registers / Watchpoints", cmd_info },
  { "x", "Scan the memory", cmd_x },
  { "p", "Calculate the value of Expression", cmd_p },
  // { "w", "", cmd_w },
  // { "d", "", cmd_d },

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_exprTEST()
{
  FILE *fp = fopen("/home/leizhenyu/opt/ysyx-workbench/nemu/tools/gen-expr/input", "r");
  if (fp == NULL) Log("init_exprTEST error");

  char expr_buff[1024] = {};    // expression
  int line_num = 0;
  int line_length = 0;

  int correct_times = 0;

  while(fgets(expr_buff, 1024, fp))
  {
    line_num++;
    line_length = strlen(expr_buff);

    if (line_length > 0 && expr_buff[line_length - 1] == '\n')
    {
      expr_buff[line_length - 1] = '\0';
    }

    char *correct_result = NULL;
    char *expression = NULL;
    char *split_index = strchr(expr_buff, ' '); // the first space is split char

    *split_index = '\0';  // split the expr_buff into two pieces

    correct_result = expr_buff;
    expression = split_index + 1;

    // BELOW: to compare the input 'cor_result' and function 'expr()'
    bool success = false;
    word_t funcExpr_result = expr(expression, &success);

    if (funcExpr_result == atoi(correct_result))
    { 
      correct_times++;
      puts("yesok");
    }

    printf("linenum: %d, funExpr_result: %u, correct result: %s\n", line_num, funcExpr_result, correct_result);
  }

  fclose(fp);

  float res = correct_times/line_num;
  printf("correct rate: %f\n", res);
  Log("expr test pass");
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  //
  init_exprTEST();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
