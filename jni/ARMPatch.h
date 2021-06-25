#include <unistd.h>
#include <dlfcn.h>

extern "C" namespace ARMPatch
{
	/*
		Get library's address
		soLib - name of a loaded library
	*/
	uintptr_t getLib(const char* soLib);
	
	/*
		Reprotect memory to allow reading/writing/executing
		addr - address
	*/
	int unprotect(uintptr_t addr);
	
	/*
		Write to memory (reprotects it)
		dest - where to start?
		src - address of an info to write
		size - size of an info
	*/
	void write(uintptr_t dest, uintptr_t src, size_t size);
	
	/*
		Read memory (reprotects it)
		src - where to read from?
		dest - where to write a readed info?
		size - size of an info
	*/
	void read(uintptr_t src, uintptr_t dest, size_t size);
	
	/*
		Place NotOperator instruction (reprotects it)
		addr - where to put
		count - how much times to put
	*/
	void NOP(uintptr_t addr, size_t count = 1);
	
	/*
		Place JUMP instruction (reprotects it)
		addr - where to put
		dest - Jump to what?
	*/
	void JMP(uintptr_t addr, uintptr_t dest);
	
	/*
		Cydia's Substrate
		addr - what to hook?
		func - Call that function instead of an original
		original - Original function!
	*/
	void hook(void* addr, void* func, void** original);
	void hook(uintptr_t addr, void* func, void** original);
}