#include "syscall.h"
#define maxlen 32

int main()
{
    int len;
    char filename[maxlen + 1];
    /*Create a file*/
    if (Create("Hello.txt") == -1)
    {
        return -1;
    }
    else
    {
        int id = Open("Hello.txt",1);
        Close(id);
        Remove("Hello.txt");
        // xuất thông báo tạo tập tin thành công
        Halt();
        return 1;
    }
}