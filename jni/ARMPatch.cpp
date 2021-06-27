#include "ARMPatch.h"
#include <cstring>
#include <sys/mman.h>

#ifdef __arm__
	extern "C" void MSHookFunction(void* symbol, void* replace, void** result);
#elif defined __aarch64__
	extern "C" void A64HookFunction(void *const symbol, void *const replace, void **result);
	#define cacheflush(c, n, zeroarg) __builtin___clear_cache((void*)c, (void*)n)
#else
	#error This lib is supposed to work on ARMv7 (ARMv7a) and ARMv8 only!
#endif

namespace ARMPatch
{
	uintptr_t getLib(const char* soLib)
	{
		return (uintptr_t)dlopen(soLib, RTLD_LAZY);
	}
	uintptr_t getSym(uintptr_t handle, const char* sym)
	{
		return (uintptr_t)dlsym((void*)handle, sym);
	}
	int unprotect(uintptr_t addr)
	{
		return mprotect((void*)(addr & 0xFFFFF000), PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
	}
	void write(uintptr_t dest, uintptr_t src, size_t size)
	{
		unprotect(dest);
		memcpy((void*)dest, (void*)src, size);
		cacheflush(dest, dest + size, 0);
	}
	void read(uintptr_t src, uintptr_t dest, size_t size)
	{
		unprotect(src);
		memcpy((void*)src, (void*)dest, size);
	}
	void NOP(uintptr_t addr, size_t count)
	{
		unprotect(addr);
		for (uintptr_t p = addr; p != (addr + count*2); p += 2)
		{
			(*(char*)p) = 0x00;
			(*(char*)(p + 1)) = 0x46;
		}
		cacheflush(addr, addr + count * 2, 0);
	}
	void JMP(uintptr_t addr, uintptr_t dest)
	{
		uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
		                   ((((dest - addr - 4) >> 1) & 0x7FF | 0xB800) << 16);
		write(addr, (uintptr_t)&newDest, 4);
	}
	void RET(uintptr_t addr)
	{
		write(addr, (uintptr_t)"\xF7\x46", 2);
	}
	void hookInternal(void* addr, void* func, void** original)
	{
		if (addr == NULL) return;
		unprotect((uintptr_t)addr);
		#ifdef __arm__
			MSHookFunction(addr, func, original);
		#elif defined __aarch64__
			A64HookFunction(addr, func, original);
		#endif
	}
	void hookPLTInternal(void* addr, void* func, void** original)
	{
		if (addr == NULL || func == NULL) return;
		unprotect((uintptr_t)addr);
		if(original != NULL) *original = addr;
		addr = func;
	}
}
