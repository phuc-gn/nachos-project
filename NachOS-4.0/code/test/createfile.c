#include "syscall.h"
#define maxlen 32

int main()
{
    int len;
    char filename[maxlen + 1];
    int temp = Create("Hello.txt");
    int id = Open("Hello.txt",0);
    Close(id);
    Remove("Hello.txt");
    Halt();
}