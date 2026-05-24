#include "util/memory.hpp"

namespace votv::memory {

uintptr_t module_base()
{
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
}

uintptr_t absolute(uintptr_t offset)
{
    return module_base() + offset;
}

bool is_readable(const void* pointer, size_t size)
{
    if (!pointer || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(pointer, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        return false;
    }

    if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS) || (mbi.Protect & PAGE_GUARD)) {
        return false;
    }

    const auto start = reinterpret_cast<uintptr_t>(pointer);
    const auto end = start + size;
    const auto region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end <= region_end;
}

} // namespace votv::memory
