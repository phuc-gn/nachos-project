#ifndef NT_H
#define NT_H

#include "copyright.h"
#include "sysdep.h"

#define MAXSIZE 20
#define RESERVED 0

class NetworkTable
{
public:
    int *fd;
    // bool *isUse;
    NetworkTable();
    ~NetworkTable();

    int SocketTCP();
    int Close(int);
    // bool isAlive(int);
    int getSID(int);
};

#endif