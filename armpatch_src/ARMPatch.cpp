#include "ARMPatch.h"
#include <ctype.h>
#include <link.h>

#ifdef __XDL
void* xdl_cached[100] {0}; // Much more enough
int cache_c = 0;

inline bool xdl_is_cached(void* ptr)
{
    for(int i = 0; i < cache_c; ++i)
        if(xdl_cached[i] == ptr) return true;
    return false;
}
inline void* xdl_cache(void* ptr)
{
    if(!xdl_is_cached(ptr))
    {
        xdl_cached[cache_c] = ptr;
        ++cache_c;
    }
    return ptr;
}
#endif

namespace ARMPatch
{
    static const char* __iter_dlName;
    static int __iter_callback(struct dl_phdr_info *info, size_t size, void *data)
    {
        if ((void *)info->dlpi_addr == data)
        {
            __iter_dlName = info->dlpi_name;
            return 1;
        }
        return 0;
    }
    
    const char* GetPatchVer()
    {
        #define STRSTRSECPASS(_AA) #_AA
        #define STRSTRMACRO(_AA) STRSTRSECPASS(_AA)
        
        #ifdef __32BIT
        return "ARMPatch v." STRSTRMACRO(ARMPATCH_VER) " (32-bit) (compiled " __DATE__ " " __TIME__ ")";
        #elif defined __64BIT
        return "ARMPatch v." STRSTRMACRO(ARMPATCH_VER) " (64-bit) (compiled " __DATE__ " " __TIME__ ")";
        #endif
    }
    int GetPatchVerInt() { return ARMPATCH_VER; }
    uintptr_t GetLib(const char* soLib)
    {
        FILE *fp = NULL;
        uintptr_t address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != NULL)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {{}
                if ( strstr( buffer, soLib ) )
                {
                    address = (uintptr_t)strtoul( buffer, NULL, 16 );
                    break;
                }
            }
            fclose(fp);
        }
        return address;
    }
    void* GetLibHandle(const char* soLib)
    {
        #ifdef __XDL
        return xdl_cache(xdl_open(soLib, XDL_DEFAULT));
        #else
        return dlopen(soLib, RTLD_LAZY);
        #endif
    }
    void* GetLibHandle(uintptr_t addr)
    {
        __iter_dlName = NULL;
        #ifdef __XDL
        xdl_iterate_phdr(__iter_callback, (void*)addr, XDL_DEFAULT);
        return xdl_cache(xdl_open(__iter_dlName, XDL_DEFAULT));
        #else
        dl_iterate_phdr(__iter_callback, (void*)addr);
        return dlopen(__iter_dlName, RTLD_LAZY);
        #endif
    }
    uintptr_t GetLibLength(const char* soLib)
    {
        FILE *fp = NULL;
        uintptr_t address = 0, end_address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != NULL)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {
                if ( strstr( buffer, soLib ) )
                {
                    const char* secondPart = strchr(buffer, '-');
                    if(!address) end_address = address = (uintptr_t)strtoul( buffer, NULL, 16 );
                    if(secondPart != NULL) end_address = (uintptr_t)strtoul( secondPart + 1, NULL, 16 );
                }
            }
            fclose(fp);
        }
        return end_address - address;
    }
    uintptr_t GetSym(void* handle, const char* sym)
    {
        #ifdef __XDL
        if(xdl_is_cached(handle))
            return (uintptr_t)xdl_sym(handle, sym, NULL);
        #endif
        return (uintptr_t)dlsym(handle, sym);
    }
    uintptr_t GetSym(uintptr_t libAddr, const char* sym)
    {
        /*Dl_info info;
        if(dladdr((void*)libAddr, &info) == 0) return 0;
        return (uintptr_t)dlsym(info.dli_fbase, sym);*/
        
        __iter_dlName = NULL;
        #ifdef __XDL
        xdl_iterate_phdr(__iter_callback, (void*)libAddr, XDL_DEFAULT);
        void* handle = xdl_cache(xdl_open(__iter_dlName, XDL_DEFAULT));
        return (uintptr_t)xdl_sym(handle, sym, NULL);
        #else
        dl_iterate_phdr(__iter_callback, (void*)libAddr);
        void* handle = dlopen(__iter_dlName, RTLD_LAZY);
        return (uintptr_t)dlsym(handle, sym);
        #endif
    }
    int Unprotect(uintptr_t addr, size_t len)
    {
        #ifdef __32BIT
        return mprotect((void*)(addr & 0xFFFFF000), len, PROT_READ | PROT_WRITE | PROT_EXEC);
        #elif defined _64BIT
        return mprotect((void*)(addr & ~(0x3UL)), len, PROT_READ | PROT_WRITE | PROT_EXEC);
        #endif
    }
    void Write(uintptr_t dest, uintptr_t src, size_t size)
    {
        Unprotect(dest, size);
        memcpy((void*)dest, (void*)src, size);
        cacheflush(dest, dest + size, 0);
    }
    void Read(uintptr_t src, uintptr_t dest, size_t size)
    {
        Unprotect(src, size); // Do we need it..?
        memcpy((void*)src, (void*)dest, size);
    }
    void WriteNOP(uintptr_t addr, size_t count)
    {
        #ifdef __32BIT
            Unprotect(addr, count * 2);
            for (uintptr_t p = addr; p != (addr + count * 2); p += 2)
            {
                (*(char*)(p + 0)) = 0x00;
                (*(char*)(p + 1)) = 0xBF;
            }
            cacheflush(addr, addr + count * 2, 0);
        #elif defined __64BIT
            Unprotect(addr, count * 4);
            for (uintptr_t p = addr; p != (addr + count * 4); p += 4)
            {
                (*(char*)(p + 0)) = 0x1F;
                (*(char*)(p + 1)) = 0x20;
                (*(char*)(p + 2)) = 0x03;
                (*(char*)(p + 3)) = 0xD5;
            }
            cacheflush(addr, addr + count * 4, 0);
        #endif
    }
    int WriteB(uintptr_t addr, uintptr_t dest) // B instruction
    {
        #ifdef __32BIT
        if(addr & 0x1)
        {
            uintptr_t calc = (int)(0.5f * (dest - addr));
            if(calc < 1) return 0; // B or B.W cant do that backwards :(
            else if(calc == 1)
            {
                WriteNOP(addr, 1);
                return 2;
            }
            
            calc -= 2;
            if(calc <= 255)
            {
                addr &= ~0x1;
            
                Unprotect(addr, 2);
                (*(unsigned char*)(addr + 0)) = (unsigned char)calc;
                (*(char*)(addr + 1)) = 0xE0;
                cacheflush(addr, addr + 2, 0);
                return 2;
            }
            else
            {
                addr &= ~0x1;
            
                Unprotect(addr, 4);
                (*(unsigned char*)(addr + 0)) = (unsigned char)calc;
                (*(char*)(addr + 1)) = 0xE0;
                cacheflush(addr, addr + 4, 0);
                
                return 4;
            }
        }
        else
        {
            return 0;
        }
        #elif defined __64BIT
            return 0;
        #endif
        /*uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                           ((((dest - addr - 4) >> 1) & 0x7FF | 0xB800) << 16);
        Write(addr, (uintptr_t)&newDest, sizeof(uintptr_t));*/
    }
    void WriteBL(uintptr_t addr, uintptr_t dest) // BL instruction
    {
        uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                           ((((dest - addr - 4) >> 1) & 0x7FF | 0xF800) << 16);
        Write(addr, (uintptr_t)&newDest, sizeof(uintptr_t));
    }
    void WriteBLX(uintptr_t addr, uintptr_t dest) // BLX instruction
    {
        uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                           ((((dest - addr - 4) >> 1) & 0x7FF | 0xE800) << 16);
        Write(addr, (uintptr_t)&newDest, sizeof(uintptr_t));
    }
    void WriteRET(uintptr_t addr)
    {
        #ifdef __32BIT
            Write(addr, (uintptr_t)"\x70\x47", 2);
            //Write(addr, (uintptr_t)"\x1E\xFF\x2F\xE1", 4);
        #elif defined __64BIT
            Write(addr, (uintptr_t)"\xC0\x03\x5F\xD6", 4);
        #endif
    }
    void Redirect(uintptr_t addr, uintptr_t to) // Was taken from TheOfficialFloW's git repo, should not work on ARM64 rn
    {
    #ifdef __32BIT
        if(!addr) return;
        uint32_t hook[2] = {0xE51FF004, to};
        if(addr & 1)
        {
            addr &= ~1;
            if (addr & 2)
            {
                WriteNOP(addr, 1); 
                addr += 2;
            }
            hook[0] = 0xF000F8DF;
        }
        Write(addr, (uintptr_t)hook, sizeof(hook));
    #elif defined __64BIT
        // TODO:
    #endif
    }
    bool hookInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func) return false;
        //Unprotect((uintptr_t)addr, 10);
        #ifdef __32BIT
            return MSHookFunction(addr, func, original);
        #elif defined __64BIT
            return A64HookFunction(addr, func, original);
        #endif
    }
    void hookPLTInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func) return;
        Unprotect((uintptr_t)addr, 8);
        if(original != NULL) *((uintptr_t*)original) = *(uintptr_t*)addr;
        *(uintptr_t*)addr = (uintptr_t)func;
    }
    
    static bool CompareData(const uint8_t* data, const bytePattern::byteEntry* pattern, size_t patternlength)
    {
        int index = 0;
        for (size_t i = 0; i < patternlength; i++)
        {
            auto byte = *pattern;
            if (!byte.bUnknown && *data != byte.nValue) return false;

            ++data;
            ++pattern;
            ++index;
        }
        return index == patternlength;
    }
    uintptr_t GetAddressFromPattern(const char* pattern, const char* soLib)
    {
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, NULL, 16);
                input += 2;
            }
            else
            {
                entry.bUnknown = true;
                input += 2;
            }
            ret.vBytes.emplace_back(entry);
        }
        
        auto patternstart = ret.vBytes.data();
        auto length = ret.vBytes.size();

        uintptr_t pMemoryBase = GetLib(soLib);
        size_t nMemorySize = GetLibLength(soLib) - length;

        for (size_t i = 0; i < nMemorySize; i++)
        {
            uintptr_t addr = pMemoryBase + i;
            if (CompareData((const uint8_t*)addr, patternstart, length)) return addr;
        }
        return (uintptr_t)0;
    }
    uintptr_t GetAddressFromPattern(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
    {
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, NULL, 16);
                input += 2;
            }
            else
            {
                entry.bUnknown = true;
                input += 2;
            }
            ret.vBytes.emplace_back(entry);
        }
        
        auto patternstart = ret.vBytes.data();
        auto length = ret.vBytes.size();

        uintptr_t scanSize = libStart + scanLen;
        for (size_t i = 0; i < scanSize; i++)
        {
            uintptr_t addr = libStart + i;
            if (CompareData((const uint8_t*)addr, patternstart, length)) return addr;
        }
        return (uintptr_t)0;
    }
    void WriteMOV(uintptr_t addr, ARMRegister from, ARMRegister to)
    {
        uint32_t newDest = (0x01 << 24) | (to << 16) | (from << 12);
        Write(addr, (uintptr_t)&newDest, sizeof(uintptr_t));
    }
}
