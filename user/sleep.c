#include "kernel/types.h"
#include "user/user.h"
int String2int(char* c)
{
    int lenth = strlen(c);
    int result = 0;
    for(int i = 0;i<lenth;i++)
    {
        if(c[i]<'0'||c[i]>'9')
        {
            fprintf(2,"%s can not to number\n",c);
            exit(1);
        }
        result = result*10 + c[i] - '0';
    }
    return result;
}
int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        fprintf(2,"Usage: sleep <number>\n");
        exit(1);
    }
    sleep(String2int(argv[1]));
    exit(0);
}