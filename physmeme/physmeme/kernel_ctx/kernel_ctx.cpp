#include "kernel_ctx.h"

namespace physmeme
{
	kernel_ctx::kernel_ctx(void* syscall_to_hook_addr)
	{
		if (psyscall_func.load() || nt_page_offset || ntoskrnl_buffer)
			return;

		nt_rva = reinterpret_cast<std::uint32_t>(
			util::get_kernel_export(
				"ntoskrnl.exe",
				syscall_hook.first,
				true
			));

		nt_page_offset = nt_rva % page_size;


		ntoskrnl_buffer = reinterpret_cast<std::uint8_t*>(
			LoadLibraryEx("ntoskrnl.exe", NULL,
				DONT_RESOLVE_DLL_REFERENCES));



		if (!is_page_found.load()) {
			psyscall_func.store(syscall_to_hook_addr);
			is_page_found.store(true);
		}


	}
			



	bool kernel_ctx::clear_piddb_cache(const std::string& file_name, const std::uint32_t timestamp)
	{
		static const auto piddb_lock =
			util::memory::get_piddb_lock();

		static const auto piddb_table =
			util::memory::get_piddb_table();

		if (!piddb_lock || !piddb_table)
			return false;

		static const auto ex_acquire_resource =
			util::get_kernel_export(
				"ntoskrnl.exe",
				"ExAcquireResourceExclusiveLite"
			);

		static const auto lookup_element_table =
			util::get_kernel_export(
				"ntoskrnl.exe",
				"RtlLookupElementGenericTableAvl"
			);

		static const auto release_resource =
			util::get_kernel_export(
				"ntoskrnl.exe",
				"ExReleaseResourceLite"
			);

		static const auto delete_table_entry =
			util::get_kernel_export(
				"ntoskrnl.exe",
				"RtlDeleteElementGenericTableAvl"
			);

		if (!ex_acquire_resource || !lookup_element_table || !release_resource)
			return false;

		PiDDBCacheEntry cache_entry;
		const auto drv_name = std::wstring(file_name.begin(), file_name.end());
		cache_entry.time_stamp = timestamp;
		RtlInitUnicodeString(&cache_entry.driver_name, drv_name.data());

		//
		// ExAcquireResourceExclusiveLite
		//
		if (!syscall<ExAcquireResourceExclusiveLite>(ex_acquire_resource, piddb_lock, true))
			return false;

		//
		// RtlLookupElementGenericTableAvl
		//
		PIDCacheobj* found_entry_ptr =
			syscall<RtlLookupElementGenericTableAvl>(
				lookup_element_table,
				piddb_table,
				reinterpret_cast<void*>(&cache_entry)
			);

		if (found_entry_ptr)
		{

			//
			// unlink entry.
			//
			PIDCacheobj found_entry = read_kernel<PIDCacheobj>(found_entry_ptr);
			LIST_ENTRY NextEntry = read_kernel<LIST_ENTRY>(found_entry.list.Flink);
			LIST_ENTRY PrevEntry = read_kernel<LIST_ENTRY>(found_entry.list.Blink);

			PrevEntry.Flink = found_entry.list.Flink;
			NextEntry.Blink = found_entry.list.Blink;

			write_kernel<LIST_ENTRY>(found_entry.list.Blink, PrevEntry);
			write_kernel<LIST_ENTRY>(found_entry.list.Flink, NextEntry);

			//
			// delete entry.
			//
			syscall<RtlDeleteElementGenericTableAvl>(delete_table_entry, piddb_table, found_entry_ptr);

			//
			// ensure the entry is 0
			//
			auto result = syscall<RtlLookupElementGenericTableAvl>(
				lookup_element_table,
				piddb_table,
				reinterpret_cast<void*>(&cache_entry)
			);

			syscall<ExReleaseResourceLite>(release_resource, piddb_lock);
			return !result;
		}
		syscall<ExReleaseResourceLite>(release_resource, piddb_lock);
		return false;
	}

	void* kernel_ctx::allocate_pool(std::size_t size, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool = 
			util::get_kernel_export(
				"ntoskrnl.exe", 
				"ExAllocatePool"
			);

		return syscall<ExAllocatePool>(
			ex_alloc_pool, 
			pool_type,
			size
		);
	}

	void* kernel_ctx::allocate_pool(std::size_t size, ULONG pool_tag, POOL_TYPE pool_type)
	{
		static const auto ex_alloc_pool_with_tag = 
			util::get_kernel_export(
				"ntoskrnl.exe", 
				"ExAllocatePoolWithTag"
			);

		return syscall<ExAllocatePoolWithTag>(
			ex_alloc_pool_with_tag,
			pool_type,
			size,
			pool_tag
		);
	}

	void kernel_ctx::read_kernel(void* addr, void* buffer, std::size_t size)
	{
		static const auto mm_copy_memory = 
			util::get_kernel_export(
				"ntoskrnl.exe", 
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>(
			mm_copy_memory,
			buffer,
			addr,
			size
		);
	}

	void kernel_ctx::write_kernel(void* addr, void* buffer, std::size_t size)
	{
		static const auto mm_copy_memory = 
			util::get_kernel_export(
				"ntoskrnl.exe",
				"RtlCopyMemory"
			);

		syscall<decltype(&memcpy)>(
			mm_copy_memory,
			addr,
			buffer,
			size
		);
	}

	void kernel_ctx::zero_kernel_memory(void* addr, std::size_t size)
	{
		static const auto rtl_zero_memory = 
			util::get_kernel_export(
				"ntoskrnl.exe",
				"RtlZeroMemory"
			);

		syscall<decltype(&RtlSecureZeroMemory)>(
			rtl_zero_memory, 
			addr,
			size
		);
	}

	PEPROCESS kernel_ctx::get_peprocess(unsigned pid) const
	{
		if (!pid)
			return {};

		PEPROCESS proc;
		static auto get_peprocess_from_pid =
			util::get_kernel_export(
				"ntoskrnl.exe",
				"PsLookupProcessByProcessId"
			);

		syscall<PsLookupProcessByProcessId>(
			get_peprocess_from_pid,
			(HANDLE)pid,
			&proc
		);
		return proc;
	}

	void* kernel_ctx::get_proc_base(unsigned pid) const
	{
		if (!pid)
			return  {};

		const auto peproc = get_peprocess(pid);

		if (!peproc)
			return {};

		static auto get_section_base = 
			util::get_kernel_export(
				"ntoskrnl.exe",
				"PsGetProcessSectionBaseAddress"
			);

		return syscall<PsGetProcessSectionBaseAddress>(
			get_section_base,
			peproc
		);
	}
}