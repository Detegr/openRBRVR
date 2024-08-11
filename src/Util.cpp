#include "Util.hpp"

void write_data(uintptr_t address, uint8_t* values, size_t byte_count)
{
    DWORD old_protection;
    void* addr = reinterpret_cast<void*>(address);

    // Change memory protection to allow writing
    if (VirtualProtect(addr, byte_count, PAGE_EXECUTE_READWRITE, &old_protection)) {
        std::memcpy(addr, values, byte_count);
        // Restore the original protection
        VirtualProtect(addr, byte_count, old_protection, &old_protection);
    } else {
        dbg("Failed to change memory protection.");
    }
}
