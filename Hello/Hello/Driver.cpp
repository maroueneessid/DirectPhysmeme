#include <ntifs.h>


NTSTATUS CustomDriverEntry(PVOID lpBaseAddress, DWORD32 dwSize)
{
	DbgPrint("> Base Address: 0x%p, Size: 0x%x", lpBaseAddress, dwSize);
	return STATUS_SUCCESS;
}
