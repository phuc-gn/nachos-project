// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void increasePC()
{
	/* set previous programm counter (debugging only)*/
	kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

	/* set next programm counter for brach execution */
	kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
}

// Input: - User space address (int)
// - Limit of buffer (int)
// Output:- Buffer (char*)
// Purpose: Copy buffer from User memory space to System memory space
char *User2System(int virtAddr, int limit)
{
	int i; // index
	int oneChar;
	char *kernelBuf = NULL;
	kernelBuf = new char[limit + 1]; // need for terminal string
	if (kernelBuf == NULL)
		return kernelBuf;
	memset(kernelBuf, 0, limit + 1);
	// printf("\n Filename u2s:");
	for (i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		// printf("%c",kernelBuf[i]);
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}

// Input: - User space address (int)
// - Limit of buffer (int)
// - Buffer (char[])
// Output:- Number of bytes copied (int)
// Purpose: Copy buffer from System memory space to User memory space
int System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0)
		return -1;
	if (len == 0)
		return len;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}
int MaxFileLength = 32;
void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
	case NoException: // Everything ok!
		kernel->interrupt->setStatus(SystemMode);
		DEBUG(dbgSys, "Switch to system mode\n");
		break;
	case PageFaultException:	// No valid translation found
	case ReadOnlyException:		// Write attempted to page marked "read-only"
	case BusErrorException:		// Translation resulted in an invalid physical address
	case AddressErrorException: // Unaligned reference or one that was beyond the end of the address space
	case OverflowException:		// Integer overflow in add or sub.
	case IllegalInstrException: // Unimplemented or reserved instr.
	case NumExceptionTypes:
		cerr << "Error " << which << " occurs\n";
		SysHalt();
		ASSERTNOTREACHED();

	case SyscallException:
		switch (type)
		{
		case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

			SysHalt();

			ASSERTNOTREACHED();
			break;

		case SC_Add:
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							/* int op2 */ (int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);

			/* Modify return point */
			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;

			ASSERTNOTREACHED();

			break;
		case SC_Create:
		{
			int virtAddr;
			char *filename;
			DEBUG(dbgSys, "\n SC_Create call ...");
			DEBUG(dbgSys, "\n Reading virtual address of filename");
			// Lấy tham số tên tập tin từ thanh ghi r4
			virtAddr = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "\n Reading filename.");
			// MaxFileLength là = 32
			filename = User2System(virtAddr, MaxFileLength + 1);
			if (filename == NULL)
			{
				printf("\n Not enough memory in system");
				DEBUG(dbgSys, "\n Not enough memory in system");
				kernel->machine->WriteRegister(2, -1); // trả về lỗi cho chương
				// trình người dùng
				increasePC();
				delete filename;
				return;
			}
			DEBUG(dbgSys, "\n Finish reading filename.");
			// DEBUG(‘a’,"\n File name : '"<<filename<<"'");
			//  Create file with size = 0
			//  Dùng đối tượng fileSystem của lớp OpenFile để tạo file,
			//  việc tạo file này là sử dụng các thủ tục tạo file của hệ điều
			//  hành Linux, chúng ta không quản ly trực tiếp các block trên
			//  đĩa cứng cấp phát cho file, việc quản ly các block của file
			//  trên ổ đĩa là một đồ án khác
			if (!kernel->fileSystem->Create(filename, 0))
			{
				printf("\n Error create file '%s'", filename);
				DEBUG(dbgSys, "\n Error:Cannot create file.");
				kernel->machine->WriteRegister(2, -1);
				increasePC();
				delete filename;
				return;
			}

			DEBUG(dbgSys, "\n File " << filename << " created successfully");
			kernel->machine->WriteRegister(2, 0); // trả về cho chương trình
												  // người dùng thành công
			increasePC();
			delete filename;

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Open:
		{
			DEBUG(dbgSys, "SC_Open call ...");
			int virtAddr = kernel->machine->ReadRegister(4);
			int type = kernel->machine->ReadRegister(5);
			char *filename;
			filename = User2System(virtAddr, MaxFileLength);
			if (filename == NULL)
			{
				printf("\n Not enough memory in system");
				DEBUG(dbgSys, "\n Not enough memory in system");
				kernel->machine->WriteRegister(2, -1); // trả về lỗi cho chương
				// trình người dùng
				increasePC();
				delete filename;
				return;
			}
			DEBUG(dbgSys, "\n Finish reading filename.");
			int freeSlot = kernel->fileSystem->FindFreeSlot();
			if (freeSlot != -1)
			{
				if (type == 0 || type == 1)
				{ // 0: read only   1:read & write
					if ((kernel->fileSystem->fileDes[freeSlot] = kernel->fileSystem->Open(filename, type)) != NULL)
					{
						DEBUG(dbgSys, "File " << filename << " opened successfully. FileID: " << freeSlot << ".\n");
						kernel->machine->WriteRegister(2, freeSlot);
					}
					else
					{
						DEBUG(dbgSys, "Cannot open file " << filename << ".\n");
						kernel->machine->WriteRegister(2, -1);
					}
				}
				else if (type == 2)
				{
					kernel->machine->WriteRegister(2, 0); // Vi tri ID 0 stdin
				}
				else if (type == 3)
				{
					kernel->machine->WriteRegister(2, 1); // Vi tri ID 1 stdout
				}
				else
				{
					// Neu khong mo duoc file
					DEBUG(dbgSys, "Cannot open file " << filename << ".\n");
					kernel->machine->WriteRegister(2, -1);
				}
			}
			else
			{
				DEBUG(dbgSys, "Cannot open file " << filename << ".\n");
				kernel->machine->WriteRegister(2, -1);
			}

			increasePC();
			delete[] filename;

			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Close:
		{
			// Doc id cua file(OpenFileID)
			DEBUG(dbgSys, "SC_Close call ...");
			int id = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "Closing file with ID: " << id << "\n");
			if (id >= 0 && id <= 19)
			{
				if (kernel->fileSystem->fileDes[id]) // neu co mo file
				{
					delete kernel->fileSystem->fileDes[id];
					kernel->fileSystem->fileDes[id] = NULL;
					kernel->machine->WriteRegister(2, 0);
					DEBUG(dbgSys, "File closed successfully."
									  << "\n");
				}
				else
				{
					DEBUG(dbgSys, "File already closed."
									  << "\n");
					kernel->machine->WriteRegister(2, 0);
				}
			}
			else
			{
				DEBUG(dbgSys, "Cannot close file. "
								  << "\n");
				kernel->machine->WriteRegister(2, -1);
			}

			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Remove:
		{
			DEBUG(dbgSys, "SC_Remove call ..."
							  << "\n");
			int virtAddr = kernel->machine->ReadRegister(4);
			char *filename;
			filename = User2System(virtAddr, MaxFileLength);
			if (filename == NULL)
			{
				DEBUG(dbgSys, "\n Not enough memory in system");
				kernel->machine->WriteRegister(2, -1); // trả về lỗi cho chương
				// trình người dùng
				increasePC();
				delete filename;
				return;
			}
			DEBUG(dbgSys, "\n Finish reading filename.");
			DEBUG(dbgSys, "Removing file " << filename << "\n");
			for (int i = 0; i < 20; i++)
			{
				if (kernel->fileSystem->fileDes[i] != NULL && strcmp(kernel->fileSystem->fileDes[i]->filename, filename) == 0)
				{
					DEBUG(dbgSys, "\n Error: File is opening. Cannot remove file.");
					kernel->machine->WriteRegister(2, -1);
					increasePC();
					delete filename;
					return;
				}
			}
			if (!kernel->fileSystem->Remove(filename))
			{
				printf("\n Error remove file '%s'", filename);
				DEBUG(dbgSys, "\n Error:Cannot remove file.");
				kernel->machine->WriteRegister(2, -1);
				increasePC();
				delete filename;
				return;
			}
			DEBUG(dbgSys, "File deleted successfully. "
							  << "\n");

			kernel->machine->WriteRegister(2, 0);
			increasePC();
			delete filename;

			return;
			ASSERTNOTREACHED();
			break;
		}
		// case SC_ReadConsole:
		// {
		// 	break;
		// }
		// case SC_WriteConsole:
		// {
		// 	break;
		// }
		case SC_Read:
		{
			// Doc id cua file(OpenFileID)
			// int id = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "Vao case SC_Read. ");
			int virtAddr = kernel->machine->ReadRegister(4);
			int size = kernel->machine->ReadRegister(5);
			int id = kernel->machine->ReadRegister(6);
			char *buffer;
			buffer = User2System(virtAddr, size);
			OpenFile *fileopen = kernel->fileSystem->fileDes[id];
			DEBUG(dbgSys, "Read file " << buffer << ". Size: " << size << ", ID:" << id << ", Type:" << fileopen->type << "\n");

			if (id < 0 || id > 19)
			{
				printf("Khong the mo file.\n");
				DEBUG(dbgSys, "Khong the read file do id file khong thuoc file descriptor table.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id] == NULL)
			{
				printf("Khong the mo file khong ton tai.\n");
				DEBUG(dbgSys, "Khong the read file KHONG ton tai.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id]->type == 3)
			{
				printf("Khong the mo file Console output.\n");
				DEBUG(dbgSys, "Khong the read file Console output..\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}

			char ch;

			// Read file bang stdin
			if (kernel->fileSystem->fileDes[id]->type == 2)
			{
				printf("Read file Console input.\n");
				printf("Nhap Buffer: \n");
				DEBUG(dbgSys, "Read file Console input.\n");
				// Convert data in buffer to 0

				// Use class SynchConsoleInput to get data from console
				// char *filename = NULL;
				// kernel->synchConsoleIn;
				// SynchConsoleInput* conslInput = new SynchConsoleInput(NULL);
				DEBUG(dbgSys, "Nhap Buffer: \n");
				cout.flush();
				int index = 0;
				do
				{
					ch = kernel->GetChar();
					if (ch != EOF && ch != '\n' && index < size)
						buffer[index] = ch;
					else
						break;
					++index;

				} while (ch != EOF && ch != '\n' && index < size);
				System2User(virtAddr, index, buffer);
				kernel->machine->WriteRegister(2, index);
				delete [] buffer;
				DEBUG(dbgSys, "Read file thanh cong\n");
				increasePC();
				return;
			}
			// truong hop Read file binh thuong
			int OldPosition = kernel->fileSystem->fileDes[id]->GetPosition();
			// Lam rong buffer
			for (int i = 0; i < size; i++)
			{
				buffer[i] = 0;
			}

			if (kernel->fileSystem->fileDes[id]->Read(buffer, size) > 0)
			{
				int NewPosition = kernel->fileSystem->fileDes[id]->GetPosition();

				System2User(virtAddr, NewPosition - OldPosition, buffer);
				kernel->machine->WriteRegister(2, NewPosition - OldPosition);

				DEBUG(dbgSys, "Read file thanh cong, So byte: " << NewPosition - OldPosition << ", Buffer: " << buffer << "\n");
			}
			else
			{
				DEBUG(dbgSys, "Read file Rong\n");
				kernel->machine->WriteRegister(2, -1);
			}
			increasePC();
			DEBUG(dbgSys, "Tang bien PC "
							  << "\n");
			delete [] buffer;
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Write:
		{
			// Doc id cua file(OpenFileID)
			// int id = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "Vao case SC_Write. ");
			// DEBUG(dbgSys, "Write file thanh cong\n");
			int virtAddr = kernel->machine->ReadRegister(4);
			int size = kernel->machine->ReadRegister(5);
			int id = kernel->machine->ReadRegister(6);
			char *buffer;
			buffer = User2System(virtAddr, size);
			OpenFile *fileopen = kernel->fileSystem->fileDes[id];

			if (id < 0 || id > 19)
			{
				printf("Khong the Write file.\n");
				DEBUG(dbgSys, "Khong the Write file do id file khong thuoc file descriptor table.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id] == NULL)
			{
				printf("Khong the Write file khong ton tai.\n");
				DEBUG(dbgSys, "Khong the Write file KHONG ton tai.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id]->type == 2)
			{
				printf("Khong the write file Console input.\n");
				DEBUG(dbgSys, "Khong the write file Console input..\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id]->type == 0)
			{
				printf("Khong the Write file Only read.\n");
				DEBUG(dbgSys, "Khong the Write file Only read..\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
			}
			// Write file with type is read and write
			int OldPosition = kernel->fileSystem->fileDes[id]->GetPosition();

			if (kernel->fileSystem->fileDes[id]->type == 1)
			{
				printf("Write file read and wite.\n");
				DEBUG(dbgSys, "Write file read and wite.\n");
				if ((kernel->fileSystem->fileDes[id]->Write(buffer, size)) > 0)
				{
					DEBUG(dbgSys, "Write Success.\n");
					int NewPosition = kernel->fileSystem->fileDes[id]->GetPosition();
					kernel->machine->WriteRegister(2, NewPosition - OldPosition);
					DEBUG(dbgSys, "Byte write: " << NewPosition - OldPosition << ".\n");
					delete[] buffer;
				}
				else
				{
					kernel->machine->WriteRegister(2, -1);
					delete [] buffer;
				}
				increasePC();
				return;
			}
			// Write Console
			if (kernel->fileSystem->fileDes[id]->type == 3)
			{

				int i = 0;
				while (buffer[i] != 0 && buffer[i] != EOF)
				{
					char ch = buffer[i];
					kernel->PushChar(ch);
					i++;
				}
				if (buffer[i] == '\n')
				{
					kernel->PushChar('\n');
				}
				kernel->machine->WriteRegister(2, i - 1);
			}
			else
			{
				kernel->machine->WriteRegister(2, -1);
			}

			increasePC();
			DEBUG(dbgSys, "Tang bien PC "
							  << "\n");
			delete [] buffer;
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Seek:
		{
			// Doc id cua file(OpenFileID)
			// int id = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "Vao case SC_Seek. \n");
			int position = kernel->machine->ReadRegister(4);
			int id = kernel->machine->ReadRegister(5);
			if (id < 0 || id > 19)
			{
				printf("Khong the Seek file.\n");
				DEBUG(dbgSys, "Khong the Seek file do id file khong thuoc file descriptor table.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (kernel->fileSystem->fileDes[id] == NULL)
			{
				printf("Khong the Seek file khong ton tai.\n");
				DEBUG(dbgSys, "Khong the Seek file KHONG ton tai.\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();

				return;
			}
			if (id == 0 || id == 1)
			{
				printf("Khong the Seek trong Console input.\n");
				DEBUG(dbgSys, "Khong the Seek  Console .\n");
				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
			}
			// Xac dinh vi tri toi da cua file
			int len = kernel->fileSystem->fileDes[id]->Length();
			// Xac dinh vi tri muon seek den

			position = (position == -1) ? len : position;
			// Xac dinh vi tri position co hop le hay khong?
			if (position > len || position < 0)
			{
				printf("\n Khong the seek den vi tri nay \n");
				kernel->machine->WriteRegister(2, -1);
			}
			else
			{
				kernel->fileSystem->fileDes[id]->Seek(position);
				DEBUG(dbgSys, "Seek file thanh cong\n");
				DEBUG(dbgSys, "Vi tri Seek den: " << position << "\n");
				kernel->machine->WriteRegister(2, position);
			}
			increasePC();
			DEBUG(dbgSys, "Tang bien PC "
							  << "\n");
			return;
			ASSERTNOTREACHED();
			break;
		}

		// network
		case SC_SocketTCP:
		{
			int sid = kernel->networkTable->SocketTCP();
			if (sid < 0)
			{

				DEBUG(dbgSys, "Failed to create socket.\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else
			{
				DEBUG(dbgSys, "Socked created. Socket ID: " << sid << ". Linux socket ID: " << kernel->networkTable->getSID(sid) << ".\n");
				kernel->machine->WriteRegister(2, sid);
			}

			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Connect:
		{
			int socket_id = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int port = kernel->machine->ReadRegister(6);

			char *ip = User2System(virtAddr, 16);

			int result = -1;
			struct sockaddr_in svr_addr;
			bzero(&svr_addr, sizeof(svr_addr));
			svr_addr.sin_family = AF_INET;
			svr_addr.sin_addr.s_addr = inet_addr(ip);
			svr_addr.sin_port = htons(port);

			if (connect(kernel->networkTable->getSID(socket_id), (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0)
			{
				DEBUG(dbgSys, "Failed to connect.\n")
				result = -1;
			}
			else
			{
				DEBUG(dbgSys, "Connected.\n");
				result = 0;
			}

			kernel->machine->WriteRegister(2, result);

			increasePC();
			delete ip;

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Send:
		{
			int socket_id = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int len = kernel->machine->ReadRegister(6);

			char *buffer = User2System(virtAddr, len);
			int result = send(kernel->networkTable->getSID(socket_id), buffer, len, 0);

			if (result < 0)
			{
				DEBUG(dbgSys, "Failed to send buffer.\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else if (result < len)
			{
				DEBUG(dbgSys, "Connection closed.\n");
				kernel->machine->WriteRegister(2, 0);
			}
			else
			{
				DEBUG(dbgSys, "Sent: " << buffer << "\n");
				kernel->machine->WriteRegister(2, result);
			}

			increasePC();
			delete [] buffer;

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Receive:
		{
			int socket_id = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int len = kernel->machine->ReadRegister(6);

			char buffer[128] = {'\0'}; // = new char[len + 1];

			int result = recv(kernel->networkTable->getSID(socket_id), buffer, len, 0);

			if (result < 0)
			{
				DEBUG(dbgSys, "Failed to receive buffer.\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else if (result < len)
			{
				DEBUG(dbgSys, "Connection closed. Received: " << buffer << "\n");
				kernel->machine->WriteRegister(2, 0);
				System2User(virtAddr, len, buffer);
			}
			else
			{
				DEBUG(dbgSys, "Received: " << buffer << "\n");
				kernel->machine->WriteRegister(2, result);
				System2User(virtAddr, len, buffer);
			}

			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Close_:
		{
			int sid = kernel->machine->ReadRegister(4);

			int result = kernel->networkTable->Close(sid);

			if (result < 0)
			{
				DEBUG(dbgSys, "Failed to close. Socet ID: " << sid << ".\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else
			{
				DEBUG(dbgSys, "Socked closed. SocID: " << sid << ".\n");
				kernel->machine->WriteRegister(2, 0);
			}

			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_ReadConsole:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			int len = kernel->machine->ReadRegister(5);

			char *buffer = User2System(virtAddr, len);
			cout << "Nhap buffer: \n";
			char ch;
			cout.flush();
			int index = 0;
			do
			{
				ch = kernel->GetChar();
				if (ch != EOF && ch != '\n' && index < len)
					buffer[index] = ch;
				else
					break;
				++index;

			} while (ch != EOF && ch != '\n' && index < len);
			System2User(virtAddr, index, buffer);

			kernel->machine->WriteRegister(2, index);
			delete [] buffer;
			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_PrintConsole:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			int len = kernel->machine->ReadRegister(5);

			char *buffer = User2System(virtAddr, len);
			printf("%s\n", buffer);

			increasePC();

			return;
			ASSERTNOTREACHED();
			break;
		}

			/////////////////////////PROJECT 2///////////////////////

		case SC_Exec:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			char *name;
			name = User2System(virtAddr, MaxFileLength);
			if (name == NULL)
			{
				DEBUG(dbgSys, "\n Not enough memory in System");
				ASSERT(false);
				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
				ASSERTNOTREACHED();
				break;
			}
			// Thực thi Exec, trả về kết quả cho thanh ghi
			kernel->machine->WriteRegister(2, SysExec(name));
			increasePC();
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Join:
		{
			int id = kernel->machine->ReadRegister(4);
			kernel->machine->WriteRegister(2, SysJoin(id));
			increasePC();
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Exit:
		{
			int id = kernel->machine->ReadRegister(4);
			kernel->machine->WriteRegister(2, SysExit(id));
			increasePC();
			return;
			ASSERTNOTREACHED();
			break;
		}
		// add by Quan :21120537
		case SC_CreateSemaphore:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			int semval = kernel->machine->ReadRegister(5);
			char *buffer;
			buffer = User2System(virtAddr, 50);
			if (buffer == NULL)
			{
				DEBUG(dbgSys, "\n buffer = NULL.");

				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
				ASSERTNOTREACHED();
				break;
			}
			int check = 0;
			check = kernel->semTab->Create(buffer, semval);
			kernel->machine->WriteRegister(2, check);
			if (check == 0)
			{
				DEBUG(dbgSys, "\n Create semaphore is successful");
			}
			else
			{
				DEBUG(dbgSys, "\n semaphore existed or cannot create semaphore.");
			}
			delete[] buffer;
			increasePC();
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Wait:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			char *buffer;
			buffer = User2System(virtAddr, 50);
			if (buffer == NULL)
			{
				DEBUG(dbgSys, "\n buffer = NULL.");

				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
				ASSERTNOTREACHED();
				break;
			}
			int check = 0;
			check = kernel->semTab->Wait(buffer);
			if (check == 0)
			{
				DEBUG(dbgSys, "\n wait semaphore.");
			}
			else
			{
				DEBUG(dbgSys, "\n Does not exists semaphore .");
			}

			kernel->machine->WriteRegister(2, check);
			increasePC();
			delete[] buffer;

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Signal:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			char *buffer;
			buffer = User2System(virtAddr, 50);
			if (buffer == NULL)
			{
				DEBUG(dbgSys, "\n buffer = NULL.");

				kernel->machine->WriteRegister(2, -1);
				increasePC();
				return;
				ASSERTNOTREACHED();
				break;
			}
			int check = 0;
			check = kernel->semTab->Signal(buffer);
			if (check == 0)
			{
				DEBUG(dbgSys, "\n Signal semaphore.");
			}
			else
			{
				DEBUG(dbgSys, "\n Does not exists semaphore. ");
			}

			kernel->machine->WriteRegister(2, check);
			increasePC();
			delete[] buffer;
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ExecV:
		{
			int argc = kernel->machine->ReadRegister(4);
			int virtAdress = kernel->machine->ReadRegister(5);
			char **argv = new char *[argc + 1];
			for (int i = 0; i < argc; i++)
			{
				int argAddress;
				kernel->machine->ReadMem(virtAdress+i*4,4,&argAddress);
				// Read and Write parameters
				char arg[MaxFileLength];
				int byteRead=0;
				do
				{
					kernel->machine->ReadMem(argAddress++,1,(int*)&arg[byteRead]);
				} while (arg[byteRead++]!='\0');
				
				printf("argv[%d]: %s \n",i,arg);
				argv[i]=strdup(arg);
			}
			argv[argc]=NULL;
			if(argv[0]==NULL)
			{
				DEBUG(dbgSys, "\n argv[0] = NULL.");
				kernel->machine->WriteRegister(2, -1);
			}
			else
			{
				// Thực thi Exec, trả về kết quả cho thanh ghi
				kernel->machine->WriteRegister(2, SysExec(argv[0]));
			}
			for(int i=0;i<argc;i++)
			{
				delete[] argv[i];
			}
			delete[] argv;
			increasePC();
			return;
			ASSERTNOTREACHED();
			break;
		}
		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}

		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
}
