#include "kernel/types.h"
#include "user/user.h"
#define ToString(x) #x
#define N 35
#define READ 0
#define WRITE 1
int DivIsOver(int target,int x)
{
    if(target % x == 0)
    {
        return 1;
    }
    return 0;
}
void prime(char c,int* head)
{
    int nail[2];
    pipe(nail);
    if(fork() == 0)
    {
        close(head[READ]);
        close(nail[WRITE]);
        char buf[1];
        if(read(nail[READ],buf,1)) prime(buf[0],nail);
        close(nail[READ]); 
        exit(0);
    }
    else
    {
        close(nail[READ]); 
        char buf[1];
        printf("prime %d\n",c-'0');
        while (read(head[READ],buf,1))
        {
            if(!DivIsOver(buf[0]-'0',c-'0'))
            {
                write(nail[WRITE],buf,1);
            }
        }
        close(head[READ]);
        close(nail[WRITE]);
        wait(0);
        exit(0);
    }

}
int main()
{
    int p[2];
    pipe(p);
    if(fork() == 0)
    {
        close(p[WRITE]);
        char buf[1];
        if(read(p[READ],buf,1)) prime(buf[0],p);
        wait(0);
        exit(0);        
    }else
    {
        close(p[READ]);
        int head = 2;
        int tail = N;
        printf("prime %d\n",head);
        for(int i = head + 1 ; i < tail ; i++)
        {
            if(!DivIsOver(i,head))
            {
                char c  = '0'+i;
                write(p[WRITE],&c,1);
            }
        }
        close(p[WRITE]);
        wait(0);
    }
    exit(0);
}