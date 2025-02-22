#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), '\0', DIRSIZ-strlen(p));
  return buf;
}
void find(char *path,char* target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  switch(st.type)
  {
    case T_DEVICE:
    case T_FILE:
      if(strcmp(fmtname(path),target) == 0) printf("find ok:%s fmtname(path):%s target:%s\n", path,fmtname(path),target);    
      break;
    case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
        printf("ls: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf+strlen(buf);
      *p++ = '/';
      while(read(fd, &de, sizeof(de)) == sizeof(de)){
       // printf("buf:%sfmtname(buf):%starget:%s %d\n",buf,fmtname(buf),target,strcmp(fmtname(buf),target));
        if(de.inum == 0 || de.name[0]== '.')
          continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
          printf("ls: cannot stat %s\n", buf);
          continue;
        }
        if(st.type == T_DIR)
        { 
//            printf("buf: %s\n",buf);
            find(buf,target);
            continue;
        } 
        if(st.type == T_FILE)
        {
          //if(strcmp(fmtname(buf),target) == 0) printf("find ok2:%s fmtname(path):%s target:%s\n", buf,fmtname(path),target);        
          if(strcmp(fmtname(buf),target) == 0) printf("%s\n",buf);
        }
      }
      break;
    }
    close(fd);

}
int main(int argc,char* argv[])
{
    if(argc < 3){
        exit(0);
      }
      printf("find begin\n");
    find(argv[1],argv[2]);
    exit(0);
}