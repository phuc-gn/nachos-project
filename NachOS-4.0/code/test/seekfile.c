#include "syscall.h"
#define maxlen 32

int main()
{
    int len;
    int fileid;
    char* filename="demo.txt";
    Create(filename);
    fileid=Open(filename,1);
    if (fileid != -1)
    {
        int position=-1;
        int index_seek=Seek(position,fileid);
        char *buffer = "123456";
        int size = 3;
        int size_buff = Write(buffer, size, fileid);
    }
    Close(fileid);
    Halt();
}