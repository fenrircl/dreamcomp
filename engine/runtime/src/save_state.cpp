// Debug checkpoint v1 (dreamcomp): full guest memory + SH-4 context + PVR registers.
//
// This is deliberately a local diagnostic format. It is not a VMU save and cannot
// be loaded by Flycast or any other emulator. Later versions add the scheduler,
// AICA/ARM7, Maple and GD-ROM sections; the header is versioned so old files fail
// cleanly instead of being misread.
#include "dream/runtime/save_state.h"

#include "dream/runtime/mem/dc_memory.h"
#include "dream/runtime/pvr/core.h"
#include "dream/runtime/system.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace dream::save {
namespace {

constexpr char kMagic[8] = {'B', 'R', 'S', 'K', 'S', 'T', '0', '2'};
constexpr std::uint32_t kVersion = 2;
constexpr std::uint32_t kPvrBytes = 0x2000;
struct Header {
    std::uint64_t frame = 0;
    std::uint32_t ctx_bytes = 0;
    std::uint32_t ram_bytes = 0;
    std::uint32_t vram_bytes = 0;
    std::uint32_t aram_bytes = 0;
    std::uint32_t bios_bytes = 0;
    std::uint32_t flash_bytes = 0;
    std::uint32_t ocram_bytes = 0;
    std::uint32_t pvr_bytes = 0;
    std::uint32_t sched_count = 0;
    std::uint64_t checksum = 0;
};

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    const std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value),
                                   static_cast<std::uint8_t>(value >> 8),
                                   static_cast<std::uint8_t>(value >> 16),
                                   static_cast<std::uint8_t>(value >> 24)};
    out.insert(out.end(), bytes, bytes + 4);
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
    append_u32(out, static_cast<std::uint32_t>(value >> 32));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& in, std::size_t at) {
    return static_cast<std::uint32_t>(in[at]) |
           (static_cast<std::uint32_t>(in[at + 1]) << 8) |
           (static_cast<std::uint32_t>(in[at + 2]) << 16) |
           (static_cast<std::uint32_t>(in[at + 3]) << 24);
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& in, std::size_t at) {
    return static_cast<std::uint64_t>(read_u32(in, at)) |
           (static_cast<std::uint64_t>(read_u32(in, at + 4)) << 32);
}

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

void set_error(std::string* error, const std::string& text) {
    if (error)
        *error = text;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes,
                std::string* error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        set_error(error, "cannot open the state file for writing: " + path);
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        set_error(error, "cannot write the state file: " + path);
        return false;
    }
    return true;
}

}  // namespace

bool save_state(const std::string& path, System& sys, pvr::Core& pvr, std::uint64_t frame,
                std::string* error) {
    Header h;
    h.frame = frame;
    h.ctx_bytes = static_cast<std::uint32_t>(sizeof(sys.ctx));
    h.ram_bytes = mem::DcMemory::kRamSize;
    h.vram_bytes = mem::DcMemory::kVramSize;
    h.aram_bytes = mem::DcMemory::kAramSize;
    h.bios_bytes = mem::DcMemory::kBiosSize;
    h.flash_bytes = mem::DcMemory::kFlashSize;
    h.ocram_bytes = mem::DcMemory::kOcramSize;
    h.pvr_bytes = kPvrBytes;
    h.sched_count = static_cast<std::uint32_t>(sys.sched.events());

    std::vector<std::uint8_t> payload;
    payload.reserve(static_cast<std::size_t>(h.ctx_bytes) + h.ram_bytes + h.vram_bytes +
                    h.aram_bytes + h.bios_bytes + h.flash_bytes + h.ocram_bytes + h.pvr_bytes +
                    8 + static_cast<std::size_t>(h.sched_count) * 8);
    const auto append = [&payload](const void* data, std::size_t size) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        payload.insert(payload.end(), p, p + size);
    };
    append(&sys.ctx, sizeof(sys.ctx));
    append(sys.memory.ram(), h.ram_bytes);
    append(sys.memory.vram(), h.vram_bytes);
    append(sys.memory.aram(), h.aram_bytes);
    append(sys.memory.bios(), h.bios_bytes);
    append(sys.memory.flash(), h.flash_bytes);
    append(sys.memory.ocram(), h.ocram_bytes);

    std::vector<std::uint32_t> regs(kPvrBytes / 4);
    pvr.save_regs(regs.data());
    append(regs.data(), kPvrBytes);

    std::uint64_t sched_now = 0;
    std::vector<std::uint64_t> deadlines;
    sys.sched.save_state(sched_now, deadlines);
    append(&sched_now, sizeof sched_now);
    if (!deadlines.empty())
        append(deadlines.data(), deadlines.size() * sizeof(std::uint64_t));
    h.checksum = checksum(payload);

    std::vector<std::uint8_t> file;
    file.insert(file.end(), kMagic, kMagic + sizeof kMagic);
    append_u32(file, kVersion);
    append_u64(file, h.frame);
    append_u32(file, h.ctx_bytes);
    append_u32(file, h.ram_bytes);
    append_u32(file, h.vram_bytes);
    append_u32(file, h.aram_bytes);
    append_u32(file, h.bios_bytes);
    append_u32(file, h.flash_bytes);
    append_u32(file, h.ocram_bytes);
    append_u32(file, h.pvr_bytes);
    append_u32(file, h.sched_count);
    append_u64(file, h.checksum);
    file.insert(file.end(), payload.begin(), payload.end());
    return write_file(path, file, error);
}

bool load_state(const std::string& path, System& sys, pvr::Core& pvr, std::uint64_t* frame,
                std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        set_error(error, "cannot open the state file: " + path);
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 8 + 4 + 8 + 8 * 4 + 8) {
        set_error(error, "state file is too short");
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) {
        set_error(error, "cannot read the state file");
        return false;
    }
    if (std::memcmp(bytes.data(), kMagic, sizeof kMagic) != 0) {
        set_error(error, "not a Berserk debug state file");
        return false;
    }
    std::size_t at = sizeof kMagic;
    const std::uint32_t version = read_u32(bytes, at);
    at += 4;
    if (version != kVersion) {
        set_error(error, "unsupported state version");
        return false;
    }
    Header h;
    h.frame = read_u64(bytes, at);
    at += 8;
    h.ctx_bytes = read_u32(bytes, at); at += 4;
    h.ram_bytes = read_u32(bytes, at); at += 4;
    h.vram_bytes = read_u32(bytes, at); at += 4;
    h.aram_bytes = read_u32(bytes, at); at += 4;
    h.bios_bytes = read_u32(bytes, at); at += 4;
    h.flash_bytes = read_u32(bytes, at); at += 4;
    h.ocram_bytes = read_u32(bytes, at); at += 4;
    h.pvr_bytes = read_u32(bytes, at); at += 4;
    h.sched_count = read_u32(bytes, at); at += 4;
    h.checksum = read_u64(bytes, at); at += 8;
    if (h.ctx_bytes != sizeof(sys.ctx) || h.ram_bytes != mem::DcMemory::kRamSize ||
        h.vram_bytes != mem::DcMemory::kVramSize || h.aram_bytes != mem::DcMemory::kAramSize ||
        h.bios_bytes != mem::DcMemory::kBiosSize || h.flash_bytes != mem::DcMemory::kFlashSize ||
        h.ocram_bytes != mem::DcMemory::kOcramSize || h.pvr_bytes != kPvrBytes ||
        h.sched_count != sys.sched.events()) {
        set_error(error, "state layout does not match this build");
        return false;
    }
    const std::uint64_t expected = static_cast<std::uint64_t>(h.ctx_bytes) + h.ram_bytes +
                                   h.vram_bytes + h.aram_bytes + h.bios_bytes + h.flash_bytes +
                                   h.ocram_bytes + h.pvr_bytes + 8 +
                                   static_cast<std::uint64_t>(h.sched_count) * 8;
    if (bytes.size() < at + expected) {
        set_error(error, "state file is truncated");
        return false;
    }
    std::vector<std::uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.end());
    if (checksum(payload) != h.checksum) {
        set_error(error, "state checksum mismatch");
        return false;
    }

    const auto copy = [&payload](std::size_t offset, void* out, std::size_t size,
                                 std::size_t* next) {
        std::memcpy(out, payload.data() + offset, size);
        *next = offset + size;
    };
    std::size_t off = 0;
    copy(off, &sys.ctx, h.ctx_bytes, &off);
    copy(off, sys.memory.ram(), h.ram_bytes, &off);
    copy(off, sys.memory.vram(), h.vram_bytes, &off);
    copy(off, sys.memory.aram(), h.aram_bytes, &off);
    copy(off, sys.memory.bios(), h.bios_bytes, &off);
    copy(off, sys.memory.flash(), h.flash_bytes, &off);
    copy(off, sys.memory.ocram(), h.ocram_bytes, &off);
    std::vector<std::uint32_t> regs(kPvrBytes / 4);
    copy(off, regs.data(), h.pvr_bytes, &off);
    pvr.restore_regs(regs.data());

    std::uint64_t sched_now = 0;
    copy(off, &sched_now, sizeof sched_now, &off);
    std::vector<std::uint64_t> deadlines(h.sched_count);
    if (!deadlines.empty())
        copy(off, deadlines.data(), deadlines.size() * sizeof(std::uint64_t), &off);
    if (!sys.sched.restore_state(sched_now, deadlines)) {
        set_error(error, "scheduler state does not match this build");
        return false;
    }
    sys.memory.mark_all_vram_dirty();
    if (frame)
        *frame = h.frame;
    return true;
}

}  // namespace dream::save
