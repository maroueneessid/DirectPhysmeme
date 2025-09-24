#include "kernel_ctx/kernel_ctx.h"
#include "drv_image/drv_image.h"
#include "raw_driver.hpp"
#include "superfetech.h"


int __cdecl main(int argc, char** argv)
{
	
	if (argc < 2)
	{
		std::perror("[!] Usage: programe.exe <driver_to_map>\n");
		return -1;
	}


	auto const mm = spf::memory_map::current();
	if (!mm) {
		printf("[-] Failed to snapshot current system memory\n");
		return -1;
	}

	void const* const tohook = util::get_kernel_export("ntoskrnl.exe", "NtShutdownSystem");

	std::uint64_t const phys = mm->translate(tohook);
	if (!phys) {
		printf("[-] Failed to translate to physical memory\n");
		return -1;
	}

	std::printf("[!] NtShutdownSystem at 0x%zX\n", tohook, phys);

	
	std::vector<std::uint8_t> drv_buffer;
	util::open_binary_file(argv[1], drv_buffer);
	if (!drv_buffer.size())
	{
		std::perror("[-] invalid drv_buffer size\n");
		return -1;
	}
	
	physmeme::drv_image image(drv_buffer);
	if (!physmeme::load_drv())
	{
		std::perror("[!] unable to load driver....\n");
		return -1;
	}
	
	physmeme::kernel_ctx kernel_ctx(reinterpret_cast<void*>(phys));
	std::printf("[+] driver has been loaded...\n");
	
	/*
	const auto drv_timestamp = util::get_file_header((void*)raw_driver)->TimeDateStamp;
	if (!kernel_ctx.clear_piddb_cache(physmeme::drv_key, drv_timestamp))
	{
		// this is because the signature might be broken on these versions of windows.
		perror("[-] failed to clear PiDDBCacheTable.\n");
		return -1;
	}
	*/
	
	const auto _get_export_name = [&](const char* base, const char* name)
	{
		return reinterpret_cast<std::uintptr_t>(util::get_kernel_export(base, name));
	};
	
	image.fix_imports(_get_export_name);
	image.map();
	
	const auto pool_base = kernel_ctx.allocate_pool(image.size(), NonPagedPool);
	image.relocate(pool_base);
	kernel_ctx.write_kernel(pool_base, image.data(), image.size());
	
	printf("[!] Driver mapped into 0x%llx\n", pool_base);
	
	auto entry_point = reinterpret_cast<std::uintptr_t>(pool_base) + image.entry_point();
	
	auto result = kernel_ctx.syscall<DRIVER_INITIALIZE>
	(
		reinterpret_cast<void*>(entry_point),
		reinterpret_cast<std::uintptr_t>(pool_base),
		image.size()
	);

	std::printf("[+] driver entry returned: 0x%p\n", result);
	
	kernel_ctx.zero_kernel_memory(pool_base, image.header_size());
	if (!physmeme::unload_drv())
	{
		std::perror("[!] unable to unload driver... all handles closed?\n");
		return -1;
	}
	
}