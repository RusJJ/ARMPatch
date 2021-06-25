#include <unistd.h>
#include <dlfcn.h>

#define DECLFN(_name, _ret, ...)			\
		_ret (*orgnl_##_name)(__VA_ARGS__);	\
		_ret hooked_##_name(__VA_ARGS__)
#define USEFN(_name)	\
		&hooked_##_name, &orgnl_##_name
#define CALLFN(_name, ...) orgnl_##_name(__VA_ARGS__)

namespace ARMPatch
{
	/*
		Get library's address
		soLib - name of a loaded library
	*/
	uintptr_t getLib(const char* soLib);
	/*
		Get library's function address by symbol (__unwind???)
		handle - handle of a library (getLib for example)
		sym - name of a function
	*/
	uintptr_t getSym(uintptr_t handle, const char* sym);
	
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
		Cydia's Substrate (use hook instead of hookInternal, ofc reprotects it!)
		addr - what to hook?
		func - Call that function instead of an original
		original - Original function!
	*/
	void hookInternal(void* addr, void* func, void** original);
	template<class A, class B, class C>
	void hook(A addr, B func, C original)
	{
		hookInternal((void*)addr, (void*)func, (void**)original);
	}
}
