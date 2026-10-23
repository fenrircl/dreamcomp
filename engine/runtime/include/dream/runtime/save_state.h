#pragma once

#include <cstdint>
#include <string>

namespace dream {
class System;
namespace aica { class Aica; }
namespace hle { class Bios; }
namespace pvr { class Core; }
namespace save {

// Debug checkpoint v7. This is a local diagnostic format, not a game save and not
// an emulator-compatible state. It covers the SH-4 context, the complete Dreamcast
// memory image (RAM/VRAM/ARAM/BIOS/flash/OCRAM), the PVR register block, the
// scheduler, AICA/ARM7/mixer playback state, GD-ROM HLE, SPG and interrupt controllers,
// plus the PVR TA parser and pending geometry stream. Maple state is outside the checkpoint.
bool save_state(const std::string& path, System& sys, pvr::Core& pvr, aica::Aica& aica,
                hle::Bios& bios, std::uint64_t frame, std::string* error);
bool load_state(const std::string& path, System& sys, pvr::Core& pvr, aica::Aica& aica,
                hle::Bios& bios, std::uint64_t* frame, std::string* error);

}  // namespace save
}  // namespace dream
