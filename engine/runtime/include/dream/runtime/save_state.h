#pragma once

#include <cstdint>
#include <string>

namespace dream {
class System;
namespace pvr { class Core; }
namespace save {

// Debug checkpoint v1. This is a local diagnostic format, not a game save and not
// an emulator-compatible state. It currently covers the SH-4 context, the complete
// Dreamcast memory image (RAM/VRAM/ARAM/BIOS/flash/OCRAM) and the PVR register
// block. Scheduler, AICA/ARM7, Maple and GD-ROM state are added in later steps.
bool save_state(const std::string& path, System& sys, pvr::Core& pvr, std::uint64_t frame,
                std::string* error);
bool load_state(const std::string& path, System& sys, pvr::Core& pvr, std::uint64_t* frame,
                std::string* error);

}  // namespace save
}  // namespace dream
