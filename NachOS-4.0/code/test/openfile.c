#include "syscall.h"
#define maxlen 32

int main()
{
    int len;
    char filename[maxlen];
    /*Create a file*/
    // Create("Hello.txt");
    int fileid = Open("Hello.txt",2);
    Close(fileid);
    Open("notfound.txt",0);
    Halt();
}