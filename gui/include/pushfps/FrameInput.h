// SPDX-License-Identifier: MIT
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors.
//
// PushFPS Frame Input API, version 1: send video frames from your own application straight into
// PushFPS, which then generates frames, enhances, records and streams them like a capture card.
// Documentation: docs/FRAME-INPUT-API.md. This header is MIT-licensed so any application, open or
// closed, can include it; PushFPS itself stays GPL-3.0. The protocol is plain Windows shared memory,
// so other languages can implement it from the documentation (tools/frame-input/send_test_pattern.py
// does it in Python).
//
// Transport: the SENDER creates a named file mapping "Local\PushFPS.FrameInput.<channel>" of
// pushfps_frame_input_mapping_size(maxWidth, maxHeight) bytes and an auto-reset event
// "Local\PushFPS.FrameInput.<channel>.Ready". The header below is at offset 0, then
// PUSHFPS_FRAME_INPUT_SLOTS pixel slots of slotBytes each, starting at offset headerBytes.
// To send a frame: pick the slot after latestSlot, set its seq to odd, write pixels and the slot
// fields, set seq to even (release), then store latestSlot and increment frameCounter, signal the
// event. PushFPS reads the newest slot and checks seq before and after copying (a seqlock), so a
// frame being written is never shown half-done. Channel names: 1-32 characters of A-Z a-z 0-9 - _.
#pragma once
#include <stdint.h>

#define PUSHFPS_FRAME_INPUT_MAGIC 0x4E494650u      /* "PFIN" little-endian */
#define PUSHFPS_FRAME_INPUT_VERSION 1u
#define PUSHFPS_FRAME_INPUT_SLOTS 3u
#define PUSHFPS_FRAME_INPUT_HEADER_BYTES 4096u
#define PUSHFPS_FRAME_INPUT_MAX_SIDE 8192u

/* Pixel formats. 8 bits per channel, top row first, rows `stride` bytes apart. */
enum pushfps_frame_input_format {
    PUSHFPS_FORMAT_BGRA8 = 1, /* B, G, R, A bytes (Windows DIB / D3D B8G8R8A8); alpha ignored */
    PUSHFPS_FORMAT_RGBA8 = 2  /* R, G, B, A bytes; alpha ignored */
};

#pragma pack(push, 8)
typedef struct pushfps_frame_input_slot {
    volatile uint32_t seq;       /* odd while the sender writes this slot, even when complete */
    uint32_t width, height;      /* pixels; at most maxWidth x maxHeight */
    uint32_t stride;             /* bytes per row, >= width * 4 */
    uint32_t format;             /* pushfps_frame_input_format */
    uint32_t flags;              /* bit 0: discontinuity (a cut, seek or restart before this frame) */
    int64_t timestamp100ns;      /* when the picture was made: QueryPerformanceCounter in 100 ns units, 0 = unknown */
    uint64_t frameIndex;         /* the sender's own counter (diagnostics) */
    uint8_t reserved[24];
} pushfps_frame_input_slot;

typedef struct pushfps_frame_input_header {
    uint32_t magic;              /* PUSHFPS_FRAME_INPUT_MAGIC */
    uint32_t version;            /* PUSHFPS_FRAME_INPUT_VERSION */
    uint32_t headerBytes;        /* PUSHFPS_FRAME_INPUT_HEADER_BYTES */
    uint32_t slotCount;          /* PUSHFPS_FRAME_INPUT_SLOTS */
    uint32_t maxWidth, maxHeight;
    uint64_t slotBytes;          /* bytes per pixel slot, >= maxWidth * maxHeight * 4 */
    uint32_t fpsNum, fpsDen;     /* nominal frame rate (e.g. 60/1); 0/0 = variable */
    uint32_t senderPid;          /* the sender's process id (PushFPS shows it and notices when it exits) */
    uint32_t colorSpace;         /* 0 = sRGB (SDR, BT.709 primaries); other values reserved */
    volatile uint32_t latestSlot;     /* index of the newest complete slot */
    uint32_t pad0;
    volatile uint64_t frameCounter;   /* frames sent; PushFPS waits for it to change */
    char senderName[64];         /* UTF-8, shown in PushFPS, NUL-terminated */
    uint8_t reserved[3776];      /* 128 bytes of fields above + 3776 + 3 x 64 bytes of slots = 4096 */
    pushfps_frame_input_slot slots[PUSHFPS_FRAME_INPUT_SLOTS];
} pushfps_frame_input_header;
#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(pushfps_frame_input_slot) == 64, "slot layout");
static_assert(sizeof(pushfps_frame_input_header) == PUSHFPS_FRAME_INPUT_HEADER_BYTES, "header layout");
#endif

/* Bytes of the whole mapping for a largest frame of maxWidth x maxHeight. */
static inline uint64_t pushfps_frame_input_mapping_size(uint32_t maxWidth, uint32_t maxHeight) {
    return (uint64_t)PUSHFPS_FRAME_INPUT_HEADER_BYTES + (uint64_t)PUSHFPS_FRAME_INPUT_SLOTS * maxWidth * maxHeight * 4u;
}

#if defined(__cplusplus) && defined(_WIN32)
// A complete sender for C++ applications (also used by PushFPS's own tests).
#include <windows.h>
#include <cstring>
#include <string>
namespace pushfps {
class FrameInputSender {
public:
    FrameInputSender() = default;
    FrameInputSender(const FrameInputSender&) = delete;
    FrameInputSender& operator=(const FrameInputSender&) = delete;
    ~FrameInputSender() { close(); }
    // Creates the channel. maxWidth x maxHeight is the largest frame you will send.
    bool open(const wchar_t* channel, uint32_t maxWidth, uint32_t maxHeight, uint32_t fpsNum = 0, uint32_t fpsDen = 0,
              const char* senderName = "") {
        close();
        if (!maxWidth || !maxHeight || maxWidth > PUSHFPS_FRAME_INPUT_MAX_SIDE || maxHeight > PUSHFPS_FRAME_INPUT_MAX_SIDE) return false;
        const std::wstring base = std::wstring(L"Local\\PushFPS.FrameInput.") + channel;
        const uint64_t size = pushfps_frame_input_mapping_size(maxWidth, maxHeight);
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, DWORD(size >> 32), DWORD(size), base.c_str());
        if (!mapping_) return false;
        view_ = static_cast<uint8_t*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, size_t(size)));
        ready_ = CreateEventW(nullptr, FALSE, FALSE, (base + L".Ready").c_str());
        if (!view_ || !ready_) { close(); return false; }
        auto* h = header();
        std::memset(h, 0, PUSHFPS_FRAME_INPUT_HEADER_BYTES);
        h->version = PUSHFPS_FRAME_INPUT_VERSION; h->headerBytes = PUSHFPS_FRAME_INPUT_HEADER_BYTES;
        h->slotCount = PUSHFPS_FRAME_INPUT_SLOTS; h->maxWidth = maxWidth; h->maxHeight = maxHeight;
        h->slotBytes = uint64_t(maxWidth) * maxHeight * 4u; h->fpsNum = fpsNum; h->fpsDen = fpsDen;
        h->senderPid = GetCurrentProcessId(); h->latestSlot = 0;
        strncpy_s(h->senderName, senderName ? senderName : "", _TRUNCATE);
        MemoryBarrier();
        h->magic = PUSHFPS_FRAME_INPUT_MAGIC;   // last: the channel is now valid
        return true;
    }
    // Sends one frame (rows top first). timestamp100ns: QueryPerformanceCounter in 100 ns, 0 = now.
    bool send(const void* pixels, uint32_t width, uint32_t height, uint32_t stride, uint32_t format, int64_t timestamp100ns = 0,
              bool discontinuity = false) {
        auto* h = header();
        if (!h || !pixels || !width || !height || width > h->maxWidth || height > h->maxHeight || stride < width * 4u ||
            (format != PUSHFPS_FORMAT_BGRA8 && format != PUSHFPS_FORMAT_RGBA8)) return false;
        if (!timestamp100ns) {
            LARGE_INTEGER now{}, freq{}; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&freq);
            timestamp100ns = int64_t((long double)now.QuadPart * 10000000.0L / (long double)freq.QuadPart);
        }
        const uint32_t index = (h->latestSlot + 1) % PUSHFPS_FRAME_INPUT_SLOTS;
        auto& slot = h->slots[index];
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&slot.seq));     // odd: writing
        uint8_t* dst = view_ + PUSHFPS_FRAME_INPUT_HEADER_BYTES + uint64_t(index) * h->slotBytes;
        for (uint32_t y = 0; y < height; ++y) std::memcpy(dst + size_t(y) * width * 4u, static_cast<const uint8_t*>(pixels) + size_t(y) * stride, size_t(width) * 4u);
        slot.width = width; slot.height = height; slot.stride = width * 4u; slot.format = format;
        slot.flags = discontinuity ? 1u : 0u; slot.timestamp100ns = timestamp100ns; slot.frameIndex = ++sent_;
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&slot.seq));     // even: complete
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&h->latestSlot), LONG(index));
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&h->frameCounter));
        SetEvent(ready_);
        return true;
    }
    void close() {
        if (view_) { header()->magic = 0; UnmapViewOfFile(view_); view_ = nullptr; }
        if (ready_) { CloseHandle(ready_); ready_ = nullptr; }
        if (mapping_) { CloseHandle(mapping_); mapping_ = nullptr; }
    }
private:
    pushfps_frame_input_header* header() const { return reinterpret_cast<pushfps_frame_input_header*>(view_); }
    HANDLE mapping_ = nullptr, ready_ = nullptr;
    uint8_t* view_ = nullptr;
    uint64_t sent_ = 0;
};
} // namespace pushfps
#endif
