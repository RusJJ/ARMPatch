#include <unistd.h>
#include <vector>
#include <dlfcn.h>
#include <sys/mman.h>

#ifdef __XDL
    #include "xdl.h"
#endif
#ifdef __USE_GLOSSHOOK
    #include "AML_PrecompiledLibs/include/Gloss.h"
#endif

#ifdef __arm__
    #define __32BIT
    #define DETHUMB(_a) ((uintptr_t)_a & ~0x1)
    #define RETHUMB(_a) ((uintptr_t)_a | 0x1)
    #define THUMBMODE(_a) (((uintptr_t)_a & 0x1)||((uintptr_t)_a & 0x2)||ARMPatch::bThumbMode||(ARMPatch::GetRecentSymAddrXDL((uintptr_t)_a) & 0x1))
    extern "C" bool MSHookFunction(void* symbol, void* replace, void** result);
#elif defined __aarch64__
    #define __64BIT
    #define DETHUMB(_a)
    #define RETHUMB(_a)
    #define THUMBMODE(_a) (false)
    extern "C" bool A64HookFunction(void *const symbol, void *const replace, void **result);
    #define cacheflush(c, n, zeroarg) __builtin___clear_cache((char*)(c), (char*)(n))
#else
    #error This lib is supposed to work on ARM only!
#endif

/* Just a hook declaration */
#define DECL_HOOK(_ret, _name, ...)                             \
    _ret (*_name)(__VA_ARGS__);                                    \
    _ret HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void */
#define DECL_HOOKv(_name, ...)                                  \
    void (*_name)(__VA_ARGS__);                                    \
    void HookOf_##_name(__VA_ARGS__)
/* Just a hook of a function */
#define HOOK(_name, _fnAddr)                                    \
    ARMPatch::Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function located in PLT section (by address!) */
#define HOOKPLT(_name, _fnAddr)                                 \
    ARMPatch::HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));

#define ARMPATCH_VER 1
    
#ifdef __32BIT
enum ARMRegister : char
{
    ARM_REG_R0 = 0,
    ARM_REG_R1,
    ARM_REG_R2,
    ARM_REG_R3,
    ARM_REG_R4,
    ARM_REG_R5,
    ARM_REG_R6,
    ARM_REG_R7,
    ARM_REG_R8,
    ARM_REG_R9,
    ARM_REG_R10,
    ARM_REG_R11,
    ARM_REG_R12,
    ARM_REG_SP,
    ARM_REG_LR,
    ARM_REG_PC,
    
    ARM_REG_INVALID
};
#elif defined __64BIT
enum ARMRegister : char
{
    ARM_REG_W0,  ARM_REG_W1,  ARM_REG_W2,  ARM_REG_W3,
    ARM_REG_W4,  ARM_REG_W5,  ARM_REG_W6,  ARM_REG_W7,
    ARM_REG_W8,  ARM_REG_W9,  ARM_REG_W10, ARM_REG_W11,
    ARM_REG_W12, ARM_REG_W13, ARM_REG_W14, ARM_REG_W15,
    ARM_REG_W16, ARM_REG_W17, ARM_REG_W18, ARM_REG_W19,
    ARM_REG_W20, ARM_REG_W21, ARM_REG_W22, ARM_REG_W23,
    ARM_REG_W24, ARM_REG_W25, ARM_REG_W26, ARM_REG_W27,
    ARM_REG_W28, ARM_REG_W29, ARM_REG_W30, ARM_REG_W31,

    ARM_REG_X0,  ARM_REG_X1,  ARM_REG_X2,  ARM_REG_X3,
    ARM_REG_X4,  ARM_REG_X5,  ARM_REG_X6,  ARM_REG_X7,
    ARM_REG_X8,  ARM_REG_X9,  ARM_REG_X10, ARM_REG_X11,
    ARM_REG_X12, ARM_REG_X13, ARM_REG_X14, ARM_REG_X15,
    ARM_REG_X16, ARM_REG_X17, ARM_REG_X18, ARM_REG_X19,
    ARM_REG_X20, ARM_REG_X21, ARM_REG_X22, ARM_REG_X23,
    ARM_REG_X24, ARM_REG_X25, ARM_REG_X26, ARM_REG_X27,
    ARM_REG_X28, ARM_REG_X29, ARM_REG_X30, ARM_REG_X31,
    
    ARM_REG_INVALID
};
#endif

struct bytePattern
{
    struct byteEntry
    {
        uint8_t nValue;
        bool bUnknown;
    };
    std::vector<byteEntry> vBytes;
};

namespace ARMPatch
{
    extern bool bThumbMode;
    const char* GetPatchVer();
    int GetPatchVerInt();
    /*
        Get library's start address
        soLib - name of a loaded library
    */
    uintptr_t GetLib(const char* soLib);
    /*
        Get library's handle
        soLib - name of a loaded library
    */
    void* GetLibHandle(const char* soLib);
    /*
        Get library's handle
        addr - an address of anything inside a library
    */
    void* GetLibHandle(uintptr_t addr);
    /*
        Get library's end address
        soLib - name of a loaded library
    */
    uintptr_t GetLibLength(const char* soLib);
    /*
        Get library's function address by symbol (__unwind???)
        handle - HANDLE (NOTICE THIS!!!) of a library (u can obtain it using dlopen)
        sym - name of a function
    */
    uintptr_t GetSym(void* handle, const char* sym);
    /*
        Get library's function address by symbol (__unwind???)
        libAddr - ADDRESS (NOTICE THIS!!!) of a library (u can obtain it using getLib)
        sym - name of a function
        @XMDS requested this
    */
    uintptr_t GetSym(uintptr_t libAddr, const char* sym);
    
    /*
        Reprotect memory to allow reading/writing/executing
        addr - address
    */
    int Unprotect(uintptr_t addr, size_t len = PAGE_SIZE);
    
    /*
        Write to memory (reprotects it)
        dest - where to start?
        src - address of an info to write
        size - size of an info
    */
    void Write(uintptr_t dest, uintptr_t src, size_t size);
    inline void Write(uintptr_t dest, const char* data)
    {
        Write(dest, (uintptr_t)data, strlen(data));
    }
    
    /*
        Read memory (reprotects it)
        src - where to read from?
        dest - where to write a readed info?
        size - size of an info
    */
    void Read(uintptr_t src, uintptr_t dest, size_t size);
    
    /*
        Place NotOPerator instruction (reprotects it)
        addr - where to put
        count - how much times to put
    */
    int WriteNOP(uintptr_t addr, size_t count = 1);
    
    /*
        Place JUMP instruction (reprotects it)
        addr - where to put
        dest - Jump to what?
    */
    int WriteB(uintptr_t addr, uintptr_t dest);
    
    /*
        Place BL instruction (reprotects it)
        addr - where to put
        dest - Jump to what?
    */
    void WriteBL(uintptr_t addr, uintptr_t dest);
    
    /*
        Place BLX instruction (reprotects it)
        addr - where to put
        dest - Jump to what?
    */
    void WriteBLX(uintptr_t addr, uintptr_t dest);
    
    /*
        Place RET instruction (RETURN, function end, reprotects it)
        addr - where to put
    */
    int WriteRET(uintptr_t addr);
    
    void WriteMOV(uintptr_t addr, uint8_t from, uint8_t to);
    
    /*
        Place LDR instruction (moves directly to the function with the same stack!)
        Very fast and very lightweight!
        addr - where to redirect
        to - redirect to what?
    */
    int Redirect(uintptr_t addr, uintptr_t to);
    
    /*
        ByteScanner
        pattern - pattern.
        soLib - library's name
    */
    uintptr_t GetAddressFromPattern(const char* pattern, const char* soLib);
    
    /*
        ByteScanner
        pattern - pattern.
        libStart - library's start address
        scanLen - how much to scan from libStart
    */
    uintptr_t GetAddressFromPattern(const char* pattern, uintptr_t libStart, uintptr_t scanLen);

    uintptr_t GetAddressFromPattern(const char* pattern, const char* soLib, const char* section);
    
    /*
        Cydia's Substrate / Rprop's Inline Hook (use hook instead of hookInternal, ofc reprotects it!)
        addr - what to hook?
        func - Call that function instead of an original
        original - Original function!
    */
    void* hookInternal(void* addr, void* func, void** original);
    template<class A, class B, class C>
    void* Hook(A addr, B func, C original)
    {
        return hookInternal((void*)addr, (void*)func, (void**)original);
    }
    template<class A, class B>
    void* Hook(A addr, B func)
    {
        return hookInternal((void*)addr, (void*)func, (void**)NULL);
    }
    
    /*
        A simple hook of a PLT-section functions (use HookPLT instead of hookPLTInternal, ofc reprotects it!)
        addr - what to hook?
        func - Call that function instead of an original
        original - Original function!
    */
    void* hookPLTInternal(void* addr, void* func, void** original);
    template<class A, class B, class C>
    void* HookPLT(A addr, B func, C original)
    {
        return hookPLTInternal((void*)addr, (void*)func, (void**)original);
    }
    template<class A, class B>
    void* HookPLT(A addr, B func)
    {
        return hookPLTInternal((void*)addr, (void*)func, (void**)NULL);
    }
    
#ifdef __USE_GLOSSHOOK
    void* hook_b(void* addr, void* func, void** original);
    void* hook_bl(void* addr, void* func, void** original);
    void* hook_blx(void* addr, void* func, void** original);
    void* hook_patch(void* addr, GlossHookPatchCallback func, bool is_4byte_jump);
#endif
    
    // xDL part
    bool IsCorrectXDLHandle(void* ptr);
    uintptr_t GetLibXDL(void* ptr);
    uintptr_t GetRecentSymAddrXDL(uintptr_t addr);
    size_t GetSymSizeXDL(uintptr_t addr);
    const char* GetSymNameXDL(uintptr_t addr);
}
