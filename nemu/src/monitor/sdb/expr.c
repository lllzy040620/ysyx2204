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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NUM, 

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces

  {"\\+", '+'},         // plus
  {"\\-", '-'},         // subtraction
  {"\\*", '*'},         // multiplication
  {"\\/", '/'},         // division
  
  {"\\(", '('},         // brackets
  {"\\)", ')'},

  {"[0-9]+", TK_NUM},    // numbers
  
  {"==", TK_EQ},        // equal
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {

        // regcomp()  ->  regexec()  ->  regfree()
        // 编译正则表达式 -> 匹配正则表达式 -> 释放正则表达式
        // int regexec (regex_t *compiled, char *string, size_t nmatch, regmatch_t matchptr [], int eflags)

        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        tokens[nr_token].type = rules[i].token_type;

        switch (rules[i].token_type)
        {
          case TK_NUM:
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
        }

        nr_token++;

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

// verify the parentheses : STACK-like solution
bool check_parentheses(int p, int q)
{
  bool res = false;
  int l_bracketsNum_toCompare = 0;

  for (int i = p; i<= q && tokens[p].type == '('; i++)
  {
    if (tokens[i].type == '(')
    {
      l_bracketsNum_toCompare++;
    }
    else if (tokens[i].type == ')')
    {
      if (i != q)
      {
        if (l_bracketsNum_toCompare >= 2)
        {
          l_bracketsNum_toCompare--;
        }
        else
        {
          res = false;
          break;
        }
      }
      else
      {
        if (l_bracketsNum_toCompare == 1)
        {
          res = true;
        }
      }
    }
  }

  return res; 
}

int find_major_op (int p, int q) // TODO: hard to calculate 123+(456+789) or illegal expression like 123+
{
  int in_brackets_level = 0; // Default : not in brackets
  int major_op_location = -1; // Init : do not exist
  int major_op_type_history = 0; // stand for current layer, 1 for '+ -', 0 for '* /'

  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == TK_NUM)
    {
      continue;
    }
    else if ((tokens[i].type == '+' || tokens[i].type == '-') && in_brackets_level == 0)
    {
      major_op_location = i;
      major_op_type_history = 1;
    }
    else if ((tokens[i].type == '*' || tokens[i].type == '/') && major_op_type_history == 0 && in_brackets_level == 0)
    {
      major_op_location = i;
    }
    else if (tokens[i].type == '(')
    {
      in_brackets_level++;
    }
    else if (tokens[i].type == ')')
    {
      in_brackets_level--;
    }
    else
    {
      // Log("Error 404");
      continue;
    }
  }

  return major_op_location;
}

// divide-and-conquer Algorithm
word_t eval(int p, int q, bool *ok)
{
  if (p > q)
  {
    Log("Error 404\n");
    *ok = false;
    assert(0);
  }
  else if (p == q)
  {
    if (tokens[p].type != TK_NUM)
    {
      Log("Bad expression\n");
      *ok = false;
    }
    else
    {
      *ok = true;
      return atoi(tokens[p].str);
    }
  }
  else if (check_parentheses(p, q) == true)
  {
    return eval(p + 1, q - 1, ok);
  }
  else
  {
    int op = find_major_op(p, q);
    int val1 = eval(p, op - 1, ok);
    int val2 = eval(op + 1, q, ok);

    switch (tokens[op].type)
    {
      case '+': return val1 + val2; break;
      case '-': return val1 - val2; break;
      case '*': return val1 * val2; break;
      case '/': return val1 / val2; break;
      default: assert(0);
    }
  }

  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0; 
  }

  // TODO: can not solve the '(2 - 1)' of spaces

  return eval(0, nr_token - 1, success);
}
