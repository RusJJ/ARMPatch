#include "ARMPatch.h"
#include <sys/mman.h>

extern "C" void MSHookFunction(void* symbol, void* replace, void** result);
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
		uint32_t newDest= 	((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
							((((dest - addr - 4) >> 1) & 0x7FF | 0xB800) << 16);
		write(addr, (uintptr_t)&newDest, 4);
	}
	void hookInternal(void* addr, void* func, void** original)
	{
		if (addr == NULL) return;
		unprotect((uintptr_t)addr);
		MSHookFunction(addr, func, original);
	}
}