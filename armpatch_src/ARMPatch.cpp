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
    bool bThumbMode = false;
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

    const char* GetPatchVerStr()
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
            {
                if ( strstr( buffer, soLib ) && strstr( buffer, "00000000" ) ) // fix android 9.0+ arm64 some sys library
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
        {
            void* ret = xdl_sym(handle, sym, NULL);
            if(!ret) ret = xdl_dsym(handle, sym, NULL);
            return (uintptr_t)ret;
        }
        #endif
        return (uintptr_t)dlsym(handle, sym);
    }

    uintptr_t GetSym(uintptr_t libAddr, const char* sym)
    {
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
        if(mprotect((void*)(addr & 0xFFFFF000), len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) return 0;
        return mprotect((void*)(addr & 0xFFFFF000), len, PROT_READ | PROT_WRITE);
        #elif defined __64BIT
        if(mprotect((void*)(addr & 0xFFFFFFFFFFFFF000), len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) return 0;
        return mprotect((void*)(addr & 0xFFFFFFFFFFFFF000), len, PROT_READ | PROT_WRITE);
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

    int WriteNOP(uintptr_t addr, size_t count)
    {
        #ifdef __32BIT
        if(THUMBMODE(addr))
        {
            addr = DETHUMB(addr);
            int bytesCount = 2 * count;
            uintptr_t endAddr = addr + bytesCount;
            Unprotect(addr, bytesCount);
            for (uintptr_t p = addr; p != endAddr; p += 2)
            {
                (*(char*)(p + 0)) = 0x00;
                (*(char*)(p + 1)) = 0xBF;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        }
        else
        {
            int bytesCount = 4 * count;
            uintptr_t endAddr = addr + bytesCount;
            Unprotect(addr, bytesCount);
            for (uintptr_t p = addr; p != endAddr; p += 4)
            {
                (*(char*)(p + 0)) = 0x00;
                (*(char*)(p + 1)) = 0xF0;
                (*(char*)(p + 2)) = 0x20;
                (*(char*)(p + 3)) = 0xE3;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        }
        #elif defined __64BIT
            int bytesCount = 4 * count;
            uintptr_t endAddr = addr + bytesCount;
            Unprotect(addr, bytesCount);
            for (uintptr_t p = addr; p != endAddr; p += 4)
            {
                (*(char*)(p + 0)) = 0x1F;
                (*(char*)(p + 1)) = 0x20;
                (*(char*)(p + 2)) = 0x03;
                (*(char*)(p + 3)) = 0xD5;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        #endif
    }

    int WriteNOP4(uintptr_t addr, size_t count)
    {
        int bytesCount = 4 * count;
        uintptr_t endAddr = addr + bytesCount;
        #ifdef __32BIT
        if(THUMBMODE(addr))
        {
            addr = DETHUMB(addr);
            endAddr = DETHUMB(endAddr);
            Unprotect(addr, bytesCount);
            for (uintptr_t p = addr; p != endAddr; p += 4)
            {
                (*(char*)(p + 0)) = 0xAF;
                (*(char*)(p + 1)) = 0xF3;
                (*(char*)(p + 2)) = 0x00;
                (*(char*)(p + 3)) = 0x80;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        }
        else
        {
            Unprotect(addr, count * 4);
            for (uintptr_t p = addr; p != endAddr; p += 4)
            {
                (*(char*)(p + 0)) = 0x00;
                (*(char*)(p + 1)) = 0xF0;
                (*(char*)(p + 2)) = 0x20;
                (*(char*)(p + 3)) = 0xE3;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        }
        #elif defined __64BIT
            Unprotect(addr, count * 4);
            for (uintptr_t p = addr; p != endAddr; p += 4)
            {
                (*(char*)(p + 0)) = 0x1F;
                (*(char*)(p + 1)) = 0x20;
                (*(char*)(p + 2)) = 0x03;
                (*(char*)(p + 3)) = 0xD5;
            }
            cacheflush(addr, endAddr, 0);
            return bytesCount;
        #endif
    }

    int WriteB(uintptr_t addr, uintptr_t dest) // B instruction
    {
        #ifdef __32BIT
        if(THUMBMODE(addr))
        {
            intptr_t calc = ((intptr_t)dest - (intptr_t)addr) / 2;
            if(calc >= -1 && calc < 1) return 0;
            else if(calc < -1)
            {
                goto do_b_w; // Only B.W supports negative offset!
            }
            else if(calc == 1)
            {
                return WriteNOP(addr, 1);
            }
            
            calc -= 2; // PC
            if(calc <= 255)
            {
                uint16_t newDest = 0xE000 | (calc & 0x7FF);
                Write(DETHUMB(addr), newDest);
                return 2;
            }
            else
            {
              do_b_w:
                // B.W goes here!
                uint32_t imm_val = (dest - addr - 4) & 0x7FFFFF;
                uint32_t newDest = ((imm_val & 0xFFF) >> 1 | 0xB800) << 16 | (imm_val >> 12 | 0xF000);

                Write(DETHUMB(addr), newDest);
                return 4;
            }
        }
        else
        {
            // Probably not working!
            uint32_t newDest = 0xEA000000 | ((dest - addr - 4) >> 2) & 0x00FFFFFF;
            Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
            return 4;
        }
        #elif defined __64BIT
            // Probably, the range is [-0xFFFFFF, 0xFFFFFFF]
            uint32_t newDest = 0x14000000 | (((dest - addr) >> 2) & 0x03FFFFFF);
            Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
            return 4;
        #endif
    }

    void WriteBL(uintptr_t addr, uintptr_t dest) // BL instruction (max. is ~0x400000 in both directions)
    {
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    if(!Gloss::Inst::MakeThumbBL(addr, dest)) Gloss::Inst::MakeThumbBL_W(addr, dest);
                }
                else
                {
                    Gloss::Inst::MakeArmBL(addr, dest);
                }
            #elif defined __64BIT
                Gloss::Inst::MakeArm64BL(addr, dest);
            #endif
        #else
            #ifdef __32BIT
                uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                                ((((dest - addr - 4) >> 1) & 0x7FF | 0xF800) << 16);
                Write(addr, (uintptr_t)&newDest, sizeof(uintptr_t));
            #elif defined __64BIT
                uint32_t newDest = 0x94000000 | (((dest - addr) >> 2) & 0x03FFFFFF);
                Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
            #endif
        #endif
    }

    void WriteBLX(uintptr_t addr, uintptr_t dest) // BLX instruction (max. is ~0x400000 in both directions)
    {
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    if(!Gloss::Inst::MakeThumbBLX(addr, dest)) Gloss::Inst::MakeThumbBLX_W(addr, dest);
                }
                else
                {
                    Gloss::Inst::MakeArmBLX(addr, dest);
                }
            #elif defined __64BIT
                __builtin_trap(); // ARMv8 doesnt have that instruction so using it is absurd!
            #endif
        #else
            #ifdef __32BIT
                uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                                ((((dest - addr - 4) >> 1) & 0x7FF | 0xE800) << 16);
                Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
            #elif defined __64BIT
                __builtin_trap(); // ARMv8 doesnt have that instruction so using it is absurd!
            #endif
        #endif
    }

    int WriteRET(uintptr_t addr)
    {
        #ifdef __32BIT
        if(THUMBMODE(addr))
        {
            Write(DETHUMB(addr), (uintptr_t)"\x70\x47", 2);
            return 2;
        }
        else
        {
            Write(addr, (uintptr_t)"\x1E\xFF\x2F\xE1", 4);
            return 4;
        }
        #elif defined __64BIT
            Write(addr, (uintptr_t)"\xC0\x03\x5F\xD6", 4);
            return 4;
        #endif
    }

    void WriteMOV(uintptr_t addr, ARMRegister from, ARMRegister to)
    {
    #ifdef __32BIT
        uint32_t newDest;
        if(THUMBMODE(addr))
        {
            newDest = (0x01 << 24) | (to << 16) | (from << 12);
        }
        else
        {
            
        }
        Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
    #elif defined __64BIT
        if((from >= ARM_REG_X0 && to < ARM_REG_X0) || (to >= ARM_REG_X0 && from < ARM_REG_X0)) return; //__builtin_trap();
        else if(from >= ARM_REG_X0)
        {
            Write(addr, ARMv8::MOVRegBits::CreateMOV(from - ARM_REG_X0, to - ARM_REG_X0, true));
        }
        else
        {
            Write(addr, ARMv8::MOVRegBits::CreateMOV(from, to, false));
        }
    #endif
    }

    int Redirect(uintptr_t addr, uintptr_t to)
    {
        if(!addr || !to) return 0;
    #ifdef __32BIT
        uint32_t hook[2] = {0xE51FF004, to};
        int ret = 8;
        if(THUMBMODE(addr))
        {
            addr &= ~0x1;
            if (addr & 0x2)
            {
                WriteNOP(RETHUMB(addr), 1); 
                addr += 2;
                ret = 10;
            }
            hook[0] = 0xF000F8DF;
        }
        Write(DETHUMB(addr), (uintptr_t)hook, sizeof(hook));
        return ret;
    #elif defined __64BIT
        Unprotect(addr, 16);
        uint64_t hook[2] = {0xD61F022058000051, to};
        Write(addr, (uintptr_t)hook, sizeof(hook));
        return 16;
    #endif
    }

    bool hookInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func) return false;
        #if defined(__USEGLOSS)
            return GlossHook(addr, func, original) != NULL;
        #elif defined(__USEDOBBY)
            return DobbyHook(addr, (dobby_dummy_func_t)func, (dobby_dummy_func_t*)original) == 0;
        #else
            #ifdef __32BIT
                return MSHookFunction(addr, func, original);
            #elif defined __64BIT
                return A64HookFunction(addr, func, original);
            #endif
        #endif
    }

    bool hookPLTInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func ||
            (original != NULL && *original == func)) return false;
        if(Unprotect((uintptr_t)addr, sizeof(uintptr_t)) != 0) return false;
        if(original != NULL) *((uintptr_t*)original) = *(uintptr_t*)addr;
        *(uintptr_t*)addr = (uintptr_t)func;
        return true;
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

    bool IsThumbAddr(uintptr_t addr)
    {
        return THUMBMODE(addr);
    }

    // xDL part
    bool IsCorrectXDLHandle(void* ptr)
    {
      #ifdef __XDL
        return xdl_is_cached(ptr);
      #endif
        return false;
    }
    uintptr_t GetLibXDL(void* ptr)
    {
      #ifdef __XDL
        xdl_info_t info;
        if (xdl_info(ptr, XDL_DI_DLINFO, &info) == -1) return 0;
        return (uintptr_t)info.dli_fbase;
      #endif
        return 0;
    }
    uintptr_t GetAddrBaseXDL(uintptr_t addr)
    {
      #ifdef __XDL
        xdl_info_t info;
        void* cache = NULL;
        if(!xdl_addr((void*)addr, &info, &cache)) return 0;
        xdl_addr_clean(&cache);
        return (uintptr_t)info.dli_saddr;
      #endif
        return 0;
    }
    size_t GetSymSizeXDL(void* ptr)
    {
      #ifdef __XDL
        xdl_info_t info;
        void* cache = NULL;
        if(!xdl_addr(ptr, &info, &cache)) return 0;
        xdl_addr_clean(&cache);
        return info.dli_ssize;
      #endif
        return 0;
    }
    const char* GetSymNameXDL(void* ptr)
    {
      #ifdef __XDL
        xdl_info_t info;
        void* cache = NULL;
        if(!xdl_addr(ptr, &info, &cache)) return NULL;
        xdl_addr_clean(&cache);
        return info.dli_sname;
      #endif
        return NULL;
    }

    // GlossHook part

    bool hookBranchInternal(void* addr, void* func, void** original)
    {
        #ifdef __USEGLOSS
            if (addr == NULL || func == NULL || addr == func) return false;
            #ifdef __32BIT
                i_set mode = $ARM;
                if (THUMBMODE(addr)) mode = $THUMB;
            #else
                i_set mode = $ARM64;
            #endif
            return GlossHookBranchB(addr, func, original, mode) != NULL;
        #else
            return false;
        #endif
    }

    bool hookBranchLinkInternal(void* addr, void* func, void** original)
    {
        #ifdef __USEGLOSS
            if (addr == NULL || func == NULL || addr == func) return false;
            #ifdef __32BIT
                i_set mode = $ARM;
                if (THUMBMODE(addr)) mode = $THUMB;
            #else
                i_set mode = $ARM64;
            #endif
            return GlossHookBranchBL(addr, func, original, mode) != NULL;
        #else
            return false;
        #endif
    }

    bool hookBranchLinkXInternal(void* addr, void* func, void** original)
    {
        #ifdef __USEGLOSS
            if (addr == NULL || func == NULL || addr == func) return false;
            #ifdef __32BIT
                if (THUMBMODE(addr)) return GlossHookBranchBLX(addr, func, original, $THUMB) != NULL;
                else return GlossHookBranchBLX(addr, func, original, $ARM) != NULL;
            #else
                __builtin_trap();
            #endif
        #else
            return false;
        #endif
    }

    uintptr_t GetBranchDest(uintptr_t addr)
    {
        #ifdef __USEGLOSS
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    addr &= ~0x1;
                    switch(Gloss::Inst::GetBranch(addr, $THUMB))
                    {
                        case Gloss::Inst::branchs::B_COND16:
                        case Gloss::Inst::branchs::B_16:
                            return (uintptr_t)Gloss::Inst::GetThumb16BranchDestination(addr);

                        case Gloss::Inst::branchs::B_COND:
                        case Gloss::Inst::branchs::B:
                        case Gloss::Inst::branchs::BL:
                        case Gloss::Inst::branchs::BLX:
                            return (uintptr_t)Gloss::Inst::GetThumb32BranchDestination(addr);

                        default:
                            return 0;
                    }
                }
                else
                {
                    return (uintptr_t)Gloss::Inst::GetArmBranchDestination(addr);
                }
            #else
                return (uintptr_t)Gloss::Inst::GetArm64BranchDestination(addr);
            #endif
        #else
            return 0;
        #endif
    }
}
