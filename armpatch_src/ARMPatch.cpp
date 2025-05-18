#include "ARMPatch.h"
#include <string.h>
#include <ctype.h>
#include <link.h>
#include <elf.h>

namespace ARMPatch
{
    bool bThumbMode = false;
    
    const char* GetPatchVerStr()
    {
        #define STRSTRSECPASS(_AA) #_AA
        #define STRSTRMACRO(_AA) STRSTRSECPASS(_AA)
        
        #ifdef __32BIT
            static constexpr const char* verstr = "ARMPatch v." STRSTRMACRO(ARMPATCH_VER) " (32-bit) (compiled " __DATE__ " " __TIME__ ")";
        #elif defined __64BIT
            static constexpr const char* verstr = "ARMPatch v." STRSTRMACRO(ARMPATCH_VER) " (64-bit) (compiled " __DATE__ " " __TIME__ ")";
        #endif
        return verstr;
    }
    int GetPatchVerInt() { return ARMPATCH_VER; }
    
    uintptr_t GetLib(const char* soLib)
    {
        FILE *fp = nullptr;
        uintptr_t address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != nullptr)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {
                if ( strstr( buffer, soLib ) && strstr( buffer, "00000000" ) ) // fix android 9.0+ arm64 some sys library
                {
                    address = (uintptr_t)strtoul( buffer, nullptr, 16 );
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
            return xdl_open(soLib, XDL_DEFAULT);
        #else
            return dlopen(soLib, RTLD_LAZY);
        #endif
    }

    void* GetLibHandle(uintptr_t libAddr)
    {
        #ifdef __XDL
            xdl_info_t info;
            void* cache = nullptr;
            if (!xdl_addr4(reinterpret_cast<void*>(libAddr), &info, &cache, XDL_NON_SYM)) return 0;
            void* handle = xdl_open(info.dli_fname, XDL_DEFAULT);
            xdl_addr_clean(&cache);
            return handle;
        #else
            Dl_info info;
            if (!dladdr(reinterpret_cast<void*>(libAddr), &info) return 0;
            return dlopen(info.dli_fname, RTLD_LAZY);
        #endif
    }
    
    void CloseLibHandle(void* handle)
    {
        #ifdef __XDL
            xdl_close(handle);
        #else
            dlclose(handle);
        #endif
    }

    uintptr_t GetLibLength(const char* soLib)
    {
        FILE *fp = nullptr;
        uintptr_t address = 0, end_address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != nullptr)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {
                if ( strstr( buffer, soLib ) )
                {
                    const char* secondPart = strchr(buffer, '-');
                    if(!address) end_address = address = (uintptr_t)strtoul( buffer, nullptr, 16 );
                    if(secondPart != nullptr) end_address = (uintptr_t)strtoul( secondPart + 1, nullptr, 16 );
                }
            }
            fclose(fp);
        }
        return end_address - address;
    }

    uintptr_t GetSym(void* handle, const char* sym)
    {
        #ifdef __XDL
            void* addr = xdl_sym(handle, sym, nullptr);
            if(!addr) addr = xdl_dsym(handle, sym, nullptr);
            return reinterpret_cast<uintptr_t>(addr);
        #else
            return reinterpret_cast<uintptr_t>(dlsym(handle, sym));
        #endif
    }

    uintptr_t GetSym(uintptr_t libAddr, const char* sym)
    {
        #ifdef __XDL
            xdl_info_t info;
            void* cache = nullptr;
            if (!xdl_addr4(reinterpret_cast<void*>(libAddr), &info, &cache, XDL_NON_SYM)) return 0;
            void* handle = xdl_open(info.dli_fname, XDL_DEFAULT);
            xdl_addr_clean(&cache);
            void* addr = xdl_sym(handle, sym, nullptr);
            if(!addr) addr = xdl_dsym(handle, sym, nullptr);
            xdl_close(handle);
            return reinterpret_cast<uintptr_t>(addr);
        #else
            Dl_info info;
            if (!dladdr(reinterpret_cast<void*>(libAddr), &info) return 0;
            void* handle = dlopen(info.dli_fname, RTLD_LAZY);
            return reinterpret_cast<uintptr_t>(dlsym(handle, sym));
        #endif
    }

    int Unprotect(uintptr_t addr, size_t len)
    {
        #if defined(__USEGLOSS)
            return SetMemoryPermission(addr, len, nullptr) ? 0 : -1; // supports 16kb pages size
        #else
            #ifdef __32BIT
                void* m ＝ reinterpret_cast<void*>(addr & 0xFFFFF000);
                if (mprotect(m, len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) return 0;
                return mprotect(m, len, PROT_READ | PROT_WRITE);
            #elif defined __64BIT
                void* m ＝ reinterpret_cast<void*>(addr & 0xFFFFFFFFFFFFF000);
                if (mprotect(m, len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) return 0;
                return mprotect(m, len, PROT_READ | PROT_WRITE);
            #endif
        #endif
    }

    void Write(uintptr_t dest, uintptr_t src, size_t size)
    {
        ARMPatch::Unprotect(dest, size);
        memcpy(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), size);
        cacheflush(dest, dest + size, 0);
    }

    void Read(uintptr_t src, uintptr_t dest, size_t size)
    {
        ARMPatch::Unprotect(dest, size);
        memcpy(reinterpret_cast<void*>(src), reinterpret_cast<void*>(dest), size);
    }

    int WriteNOP(uintptr_t addr, size_t count)
    {
        #ifdef __32BIT
            if(THUMBMODE(addr))
            {
                addr = DETHUMB(addr);
                int bytesCount = 2 * count;
                uintptr_t endAddr = addr + bytesCount;
                ARMPatch::Unprotect(addr, bytesCount);
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
                ARMPatch::Unprotect(addr, bytesCount);
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
            ARMPatch::Unprotect(addr, bytesCount);
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
                ARMPatch::Unprotect(addr, bytesCount);
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
                ARMPatch::Unprotect(addr, count * 4);
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
            ARMPatch::Unprotect(addr, count * 4);
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
    
    void WriteMOV(uintptr_t addr, ARMRegister from, ARMRegister to, bool is_t16)
    {
    #ifdef __32BIT
        if(THUMBMODE(addr))
        {
            if (is_t16)
            {
                // TODO thumb 2
            }
            else
            {
                addr = DETHUMB(addr);
                uint32_t newDest = (0x01 << 24) | (to << 16) | (from << 12);
                Write(addr, (uintptr_t)&newDest, sizeof(uint32_t));
            }
        }
        else
        {
            // TODO arm
        }
    #elif defined __64BIT
        (void)is_t16;
        if((from >= ARM_REG_X0 && to < ARM_REG_X0) || (to >= ARM_REG_X0 && from < ARM_REG_X0)) return; //__builtin_trap();
        else if(from >= ARM_REG_X0)
        {
            Write(addr, ARMv8::MOVRegBits::Create(from - ARM_REG_X0, to - ARM_REG_X0, true));
        }
        else
        {
            Write(addr, ARMv8::MOVRegBits::Create(from, to, false));
        }
    #endif
    }

    int Redirect(uintptr_t addr, uintptr_t to, bool _4byte)
    {
        if(!addr || !to) return 0;
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                i_set mode = I_ARM;
                if (THUMBMODE(addr)) mode = I_THUMB;
            #else
                i_set mode = I_ARM64;
            #endif
            void* ret = GlossHookRedirect(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(to), _4byte, mode);
            size_t len = 0;
            GlossHookGetBackupsInst(ret, &len);
            return len;
        #else
            (void)_4byte;
            #ifdef __32BIT
                uint32_t hook[2] = {0xE51FF004, to};
                int ret = 8;
                if (THUMBMODE(addr))
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
                ARMPatch::Unprotect(addr, 16);
                uint64_t hook[2] = {0xD61F024058000052, to};
                Write(addr, (uintptr_t)hook, sizeof(hook));
                return 16;
            #endif
        #endif
    }

    bool hookInternal(void* addr, void* func, void** original, bool _4byte)
    {
        if (addr == nullptr || func == nullptr || addr == func) return false;
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                i_set mode = I_ARM;
                if (THUMBMODE(addr)) mode = I_THUMB;
            #else
                i_set mode = I_ARM64;
            #endif
            if (_4byte) 
                return GlossHookAddr(addr, func, original, _4byte, mode) != nullptr;
            else
                return GlossHook(addr, func, original) != nullptr;
        #elif defined(__USEDOBBY)
            (void)_4byte;
            return DobbyHook(addr, (dobby_dummy_func_t)func, (dobby_dummy_func_t*)original) == 0;
        #else
            (void)_4byte;
            #ifdef __32BIT
                return MSHookFunction(addr, func, original);
            #elif defined __64BIT
                return A64HookFunction(addr, func, original);
            #endif
        #endif
    }

    bool hookPLTInternal(void* addr, void* func, void** original)
    {
        if (addr == nullptr || func == nullptr || addr == func || (original != nullptr && *original == func)) return false;
        #if defined(__USEGLOSS)
            return GlossGotHook(addr, func, original) != nullptr;
        #else
            if (ARMPatch::Unprotect((uintptr_t)addr, sizeof(uintptr_t)) != 0) return false;
            if(original != nullptr) *((uintptr_t*)original) = *(uintptr_t*)addr;
            *(uintptr_t*)addr = (uintptr_t)func;
            return true;
        #endif
    }

    static bool CompareData(const uint8_t* data, const bytePattern::byteEntry* pattern, size_t patternlength)
    {
        uint32_t index = 0;
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
        if (pattern == nullptr || soLib == nullptr) return 0;
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, nullptr, 16);
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
        
        #if defined(__USEGLOSS)
            uintptr_t pMemoryBase = 1;
            size_t nMemorySize = 0;
            uint32_t nMemoryType = PT_NULL;
            for (uint16_t id = 0; pMemoryBase != 0; id++)
            {
                pMemoryBase = GlossGetLibSegment(soLib, id, &nMemoryType, &nMemorySize);
                if (nMemoryType == PT_LOAD && pMemoryBase != 0)
                {
                    nMemorySize -= length;
                    for (size_t i = 0; i < nMemorySize; i++)
                    {
                        uintptr_t addr = pMemoryBase + i;
                        if (CompareData((const uint8_t*)addr, patternstart, length)) return addr;
                    }
                }
            }
        #else
            uintptr_t pMemoryBase = GetLib(soLib);
            size_t nMemorySize = GetLibLength(soLib) - length;
            for (size_t i = 0; i < nMemorySize; i++)
            {
                uintptr_t addr = pMemoryBase + i;
                if (CompareData((const uint8_t*)addr, patternstart, length)) return addr;
            }
        #endif
        return 0;
    }

    uintptr_t GetAddressFromPattern(const char* pattern, uintptr_t libStart, size_t scanLen)
    {
        if (pattern == nullptr || !libStart || !scanLen) return 0;
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, nullptr, 16);
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

        uintptr_t scanSize = scanLen - length;
        for (size_t i = 0; i < scanSize; i++)
        {
            uintptr_t addr = libStart + i;
            if (CompareData((const uint8_t*)addr, patternstart, length)) return addr;
        }
        return 0;
    }
    
    bool IsThumbAddr(uintptr_t addr)
    {
        return THUMBMODE(addr);
    }

    // xDL part
    uintptr_t GetLibXDL(void* handle)
    {
      #ifdef __XDL
        xdl_info_t info;
        if (xdl_info(handle, XDL_DI_DLINFO, &info) == -1) return 0;
        return reinterpret_cast<uintptr_t>(info.dli_fbase);
      #endif
        return 0;
    }
    
    uintptr_t GetSymAddrXDL(uintptr_t libaddr)
    {
      #ifdef __XDL
        xdl_info_t info;
        void* cache = nullptr;
        if (!xdl_addr(reinterpret_cast<void*>(libaddr), &info, &cache)) return 0;
        uintptr_t addr = reinterpret_cast<uintptr_t>(info.dli_saddr);
        xdl_addr_clean(&cache);
        return addr;
      #endif
        return 0;
    }
    
    size_t GetSymSizeXDL(uintptr_t libaddr)
    {
      #ifdef __XDL
        xdl_info_t info;
        void* cache = nullptr;
        if (!xdl_addr(reinterpret_cast<void*>(libaddr), &info, &cache)) return 0;
        size_t size = info.dli_ssize;
        xdl_addr_clean(&cache);
        return size;
      #endif
        return 0;
    }
    
    const char* GetSymNameXDL(uintptr_t libaddr)
    {
      #ifdef __XDL
        static char name[255];
        memset(name, 0, sizeof(name));
        xdl_info_t info;
        void* cache = nullptr;
        if (!xdl_addr(reinterpret_cast<void*>(libaddr), &info, &cache)) return 0;
        strlcpy(name, info.dli_sname, sizeof(name) - 1);
        xdl_addr_clean(&cache);
        return name;
      #endif
        return nullptr;
    }

    // GlossHook part
    int WriteB(uintptr_t addr, uintptr_t dest, bool is_t16) // B instruction
    {
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    if (is_t16)
                    {
                        Gloss::Inst::MakeThumb16B(addr, dest);
                        return 2;
                    }
                    else
                    {
                        Gloss::Inst::MakeThumb32B(addr, dest);
                        return 4;
                    }
                }
                else
                {
                    Gloss::Inst::MakeArmB(addr, dest);
                    return 4;
                }
            #elif defined __64BIT
                (void)is_t16;
                Gloss::Inst::MakeArm64B(addr, dest);
                return 4;
            #endif
        #endif
    }

    void WriteBL(uintptr_t addr, uintptr_t dest, bool is_width) // BL instruction
    {
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    if (is_width)
                    {
                        Gloss::Inst::MakeThumbBL_W(addr, dest);
                    }
                    else
                    {
                        Gloss::Inst::MakeThumbBL(addr, dest);
                    }
                }
                else
                {
                    Gloss::Inst::MakeArmBL(addr, dest);
                }
            #elif defined __64BIT
                (void)is_width;
                Gloss::Inst::MakeArm64BL(addr, dest);
            #endif
        #endif
    }

    void WriteBLX(uintptr_t addr, uintptr_t dest, bool is_width) // BLX instruction
    {
        #if defined(__USEGLOSS)
            #ifdef __32BIT
                if(THUMBMODE(addr))
                {
                    if (is_width)
                    {
                        Gloss::Inst::MakeThumbBLX_W(addr, dest);
                    }
                    else
                    {
                        Gloss::Inst::MakeThumbBLX(addr, dest);
                    }
                }
                else
                {
                    Gloss::Inst::MakeArmBLX(addr, dest);
                }
            #elif defined __64BIT
                __builtin_trap(); // ARMv8 doesnt have that instruction so using it is absurd!
            #endif
        #endif
    }
    
    uintptr_t* GetGot(void* handle, const char* sym, size_t* size)
    {
        #if defined(__USEGLOSS)
            uintptr_t* addr_list = nullptr;
            if (GlossGot(handle, sym, &addr_list, size)) return addr_list;
        #endif
        return nullptr;
    }
    
    uintptr_t GetAddressFromPattern(const char* pattern, const char* soLib, const char* section)
    {
        #if defined(__USEGLOSS)
            if (pattern == nullptr || soLib == nullptr) return 0;
            size_t sec_size = 0;
            uintptr_t sec_addr = GlossGetLibSection(soLib, section, &sec_size);
            if (sec_addr != 0)
                return GetAddressFromPattern(pattern, sec_addr, sec_size);
        #endif
        return 0;
    }
    
    bool hookBranchLinkInternal(void* addr, void* func, void** original) // BL Hook
    {
        #ifdef __USEGLOSS
            if (addr == nullptr || func == nullptr || addr == func) return false;
            #ifdef __32BIT
                i_set mode = I_ARM;
                if (THUMBMODE(addr)) mode = I_THUMB;
            #else
                i_set mode = I_ARM64;
            #endif
            return GlossHookBranchBL(addr, func, original, mode) != nullptr;
        #else
            return false;
        #endif
    }

    bool hookBranchLinkXInternal(void* addr, void* func, void** original) // BLX Hook
    {
        #ifdef __USEGLOSS
            if (addr == nullptr || func == nullptr || addr == func) return false;
            #ifdef __32BIT
                if (THUMBMODE(addr)) return GlossHookBranchBLX(addr, func, original, I_THUMB) != nullptr;
                else return GlossHookBranchBLX(addr, func, original, I_ARM) != nullptr;
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
                    switch(Gloss::Inst::GetBranch(addr, I_THUMB))
                    {
                        case Gloss::Inst::Branchs::B_COND16:
                        case Gloss::Inst::Branchs::B_16:
                            return (uintptr_t)Gloss::Inst::GetThumb16BranchDestination(addr);

                        case Gloss::Inst::Branchs::B_COND:
                        case Gloss::Inst::Branchs::B:
                        case Gloss::Inst::Branchs::BL:
                        case Gloss::Inst::Branchs::BLX:
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
