#pragma once

#include <Windows.h>

#include <cstdint>

namespace votv::memory {

uintptr_t module_base();
uintptr_t absolute(uintptr_t offset);
bool is_readable(const void* pointer, size_t size = sizeof(void*));

} // namespace votv::memory
