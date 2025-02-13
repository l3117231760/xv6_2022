#include "kernel/types.h"
#include "user/user.h"
int main()
{
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);
    int pid = fork();
    if(pid == 0)
    {
        // printf("chi\n");
        char buf[2];
        read(p1[0],buf,1);
        if(buf[0] == '2')
        {
            printf("%d: received ping\n",getpid());
            write(p2[1],"1",1);
            exit(0);
        }else
        {
            fprintf(2,"%cchrid error\n",buf[0]);
            exit(-1);
        }
    }
    else
    {
       // printf("pare\n");
       write(p1[1],"2",2);
       char buf[1];
       read(p2[0],buf,1);
       if(buf[0] == '1')
       {
            printf("%d: received pong\n",getpid());
            exit(0);
       }else
       {
        fprintf(2,"%cparent error\n",buf[0]);
        exit(-1);
       }
    }
    exit(0);
}