#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAX_LEN 100
int main(int argc,char *argvs[])
{
    // int x = 0;
    if(argc < 2) // xargc commend
    {
        fprintf(2,"commmend is less\n");
        exit(1);
    }
    
    char commmend[MAX_LEN];
    strcpy(commmend,argvs[1]);
    printf("commmend :%s\n",commmend);
    char my_argvs[MAXARG][MAX_LEN];
    char* exec_argv[MAXARG];
    for(int i = 0;i < MAXARG ; i++)
    {
        exec_argv[i] = 0;
    }

    int count  = 0;
    int len = 0;
    char c[1];
    int i =12; // xargs is beside

    for (i = 1 ;i < argc ;i++)
    {
        strcpy(my_argvs[count],argvs[i]);
        // printf("argvs[%d]:%s\n",count,my_argvs[count]);
        count++;
    }


    while (read(0,c,1))
    {
        if(c[0] == '\n')
        {
            count++;
            break;
        }
        if(c[0] == ' ')
        {
            count++;
            len = 0;
            continue;
        }
        my_argvs[count][len] = c[0];
        len++;
    }

    // for (i = 0 ;i < count ;i++)
    // {
    //     printf("argvs[%d]:%s\n",i,my_argvs[i]);
    // }

    for(int i = 0; i< count; i++)
    {
        exec_argv[i] = my_argvs[i];
    }

    for (i = 0 ;i < count ;i++)
    {
        printf("exec_argv[%d]:%s\n",i,exec_argv[i]);
    }

    if(fork() == 0)
    {
        exec(commmend,exec_argv);
        exit(0);
    }else
    {
        wait(0);
    }
    exit(0);
}
//  find . ls | xargs1 ls 1
// echo hello too | xargs1 echo bye