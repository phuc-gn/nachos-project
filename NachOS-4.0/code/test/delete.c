#include "syscall.h"
#define maxlen 32

int main()
{
    int len;
    char filename[maxlen + 1];
    /*Create a file*/
    if (Remove("openfile.c") == -1)
    {
        // xuất thông báo lỗi tạo tập tin
    }
    else
    {
        // xuất thông báo tạo tập tin thành công
    }
    Halt();
}