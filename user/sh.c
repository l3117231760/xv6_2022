// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};
// execve 命令
struct execcmd {
  int type;             // 两个一组
  char *argv[MAXARGS];  // 参数起始地址     个数最多为10
  char *eargv[MAXARGS]; // 参数结束地址     最多为10个 
};
// 重定向文件
struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;  // 文件名起始
  char *efile; // 文件名结束
  int mode;
  int fd;     // stdin or stdout
};
// pipe
struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));  // __attribute__((noreturn)) 在函数内部有 return 语句，编译器就会发出警告。这样可以帮助你发现潜在的逻辑错误。

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];  // 存放管道的 fd
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv); // exec("/bin/echo", argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST: // ls ; pwd
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){   // ...|
      close(1);     //  关闭标准输出
      dup(p[1]);    //  把管道的输出复制一份作为标准输出
      close(p[0]);  // 关闭管道的标准输入
      close(p[1]);  // 关闭管道的标准输出（不关闭的话可能导致无法感知...）
      runcmd(pcmd->left);
    }
    if(fork1() == 0){   // | ...
      close(0);   //  关闭标准输入 
      dup(p[0]);  //  复制一份管道的标准输入作为自己的标准输入
      close(p[0]);//  关闭管道的标准输入
      close(p[1]);//  关闭管道的标准输出
      runcmd(pcmd->right);
    }
    close(p[0]); 
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK: // & 
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd); 
    break;// 没有wait（0）
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  write(2, "$ ", 2);    // 0标准输入 1标准输出 2标准错误输出 
  memset(buf, 0, nbuf); // 清空标准输入缓冲区
  gets(buf, nbuf);      // 
  if(buf[0] == 0)       // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0) // cd + ' '
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors
// 返回一个 cmd->type = EXEC 的堆区变量
struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}
// -->
struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}
// // <-- -->
struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}
// <-- -->
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}
// -->
struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";
/*
  返回值
  1. 英文字母    : a
  2. "<|>&;()" : 返回对应的字母
  3. ">>"      :  + 
*/
/*
  具体操作 （q == 删除起始字符的字符串 eq == 执行 1 2.1 2.2 后的字符串）(eq 删除单词后没有删除其后面的空格)
  1. 删除起始为 " \t\r\n\v" 的所有字符
  2.1 如果删除后起始字符为 "<|>&;()" 以及 ">>" 也进行删除
  2.2 如果删除后的起始字符为英文字母则删除第一个单词
  3. 结束后再次删除为 " \t\r\n\v" 的所有字符
*/
int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  // s(*ps) 删除掉 " \t\r\n\v"
  while(s < es && strchr(whitespace, *s))
    s++;

  if(q) // 如果 q 不为空 q 为 ps删除掉 " \t\r\n\v"
    *q = s;
  ret = *s; // 返回值为第一个字符默认的是 但是 ">>"的返回值为 +
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++; // 开头是上面几个符号直接跳过
    break;
  case '>':
    s++; // > 直接覆盖文件
    if(*s == '>'){ // >> 添加在文件末尾
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}
// 1. 去掉 ps 指向字符串 开头的 " \t\r\n\v"（包括空格）
// 2. s的第一个字符不为第三个参数(toks)的任意一个字符且不为'\0'就会返回 true else flase
int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  // 去除掉了开头 " \t\r\n\v" 空格也被包括了 这些字符
  // s ==  " \t\r\n\v"（包括）的其中一个s就会 ++
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  // printf("peek : %s\n",*ps);
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);


/*
//  struct cmd {
//    int type;
//  };
*/

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;
  
  es = s + strlen(s);     // es == '\0'e
  // es为最后一个字符的地址
  // printf("%s\n",s);
  // printf("%c\n",*s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  // 将所有已统计的字符串都以空字符（NULL，即 '\0'）结尾
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  
  // ｜ 的优先级高于 ；&  
  cmd = parsepipe(ps, es); 
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}
 // 首字母为 "<>"会进入
struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;
  /*  if(i == 0)
    {
    printf("\nout:%s%d\n",*ps,i);
    if(peek(ps, es, "<>"))
    {
      printf("true\n");
    } else
    {
      printf("flase\n");
    };
    i++;
    }
  */
  while(peek(ps, es, "<>")){
    //printf("\nin:%s%d\n",*ps,i++);
    tok = gettoken(ps, es, 0, 0);
    // printf("tok:%c\n",tok); 
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0); //删除掉(
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  // printf("%s\n",*ps);
  if(peek(ps, es, "("))   // 处理括号分组（如 `(ls; echo) | wc`）
    return parseblock(ps, es);

  ret = execcmd(); 
  cmd = (struct execcmd*)ret;
  // printf("%s\n",*ps);
  argc = 0;
  ret = parseredirs(ret, ps, es); 
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
