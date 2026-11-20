// NLFork: PushFPS Frame Input sender, see pushfpsoutput.h. The protocol (version 1) is described in
// include/pushfps/FrameInput.h (MIT, from PushFPS): a named file mapping with a 4096-byte header and
// three pixel slots, a seqlock per slot, and an auto-reset event signalled for every new frame.
#include "pushfpsoutput.h"

int64_t PushFpsOutput::smoothTimestamp(int64_t arrival, int64_t step, int64_t interval, int64_t &last, bool &have_last)
{
	int64_t ts = arrival;
	if (have_last && step > 0 && interval > 0) {
		const int64_t predicted = last + step;
		if (arrival > predicted && arrival - predicted <= 3 * interval)
			ts = predicted + (arrival - predicted) / 16; // late: keep the rhythm, catch up slowly
		// on time or early: its own arrival; more than 3 frames behind the clock: start over
	}
	if (have_last && ts <= last)
		ts = last + 1;
	last = ts;
	have_last = true;
	return ts;
}

#ifdef _WIN32
#include "pushfps/FrameInput.h"

#include <windows.h>
#include <cstring>
#include <string>

namespace {
pushfps_frame_input_header *headerOf(uint8_t *view) { return reinterpret_cast<pushfps_frame_input_header *>(view); }
}

PushFpsOutput::~PushFpsOutput()
{
	close();
}

int64_t PushFpsOutput::nowTimestamp100ns()
{
	LARGE_INTEGER now{}, freq{};
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&freq);
	if (freq.QuadPart <= 0)
		return 0;
	// Whole seconds and remainder separately so the multiplication can't overflow
	const int64_t secs = now.QuadPart / freq.QuadPart;
	const int64_t rem = now.QuadPart % freq.QuadPart;
	return secs * 10000000LL + (rem * 10000000LL) / freq.QuadPart;
}

bool PushFpsOutput::open(uint32_t fps_num, uint32_t fps_den, const char *sender_name, const wchar_t *channel)
{
	close();
	const std::wstring name = std::wstring(L"Local\\PushFPS.FrameInput.") + (channel && *channel ? channel : L"default");
	const uint64_t size = pushfps_frame_input_mapping_size(kMaxWidth, kMaxHeight);
	HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
	                              static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), name.c_str());
	if (!m)
		return false;
	auto *v = static_cast<uint8_t *>(MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<size_t>(size)));
	HANDLE e = CreateEventW(nullptr, FALSE, FALSE, (name + L".Ready").c_str());
	if (!v || !e) {
		if (v)
			UnmapViewOfFile(v);
		if (e)
			CloseHandle(e);
		CloseHandle(m);
		return false;
	}
	mapping = m;
	ready = e;
	view = v;
	auto *h = headerOf(view);
	// The mapping may still exist from an earlier run (PushFPS keeps it open): invalidate it
	// first, then fill the header, and write the magic last so PushFPS only sees a complete one.
	InterlockedExchange(reinterpret_cast<volatile LONG *>(&h->magic), 0);
	MemoryBarrier();
	std::memset(h, 0, PUSHFPS_FRAME_INPUT_HEADER_BYTES);
	h->version = PUSHFPS_FRAME_INPUT_VERSION;
	h->headerBytes = PUSHFPS_FRAME_INPUT_HEADER_BYTES;
	h->slotCount = PUSHFPS_FRAME_INPUT_SLOTS;
	h->maxWidth = kMaxWidth;
	h->maxHeight = kMaxHeight;
	h->slotBytes = static_cast<uint64_t>(kMaxWidth) * kMaxHeight * 4u;
	h->fpsNum = fps_num;
	h->fpsDen = fps_den;
	h->senderPid = GetCurrentProcessId();
	h->colorSpace = 0; // sRGB, SDR
	h->latestSlot = 0;
	if (sender_name) {
		std::strncpy(h->senderName, sender_name, sizeof(h->senderName) - 1);
		h->senderName[sizeof(h->senderName) - 1] = '\0';
	}
	MemoryBarrier();
	InterlockedExchange(reinterpret_cast<volatile LONG *>(&h->magic), static_cast<LONG>(PUSHFPS_FRAME_INPUT_MAGIC));
	sent = 0;
	pending_slot = -1;
	have_timestamp = false;
	last_content = -1;
	return true;
}

void PushFpsOutput::close()
{
	if (view) {
		if (pending_slot >= 0)
			cancelFrame();
		InterlockedExchange(reinterpret_cast<volatile LONG *>(&headerOf(view)->magic), 0);
		UnmapViewOfFile(view);
		view = nullptr;
	}
	if (ready) {
		CloseHandle(static_cast<HANDLE>(ready));
		ready = nullptr;
	}
	if (mapping) {
		CloseHandle(static_cast<HANDLE>(mapping));
		mapping = nullptr;
	}
	pending_slot = -1;
}

void PushFpsOutput::setFps(uint32_t fps_num, uint32_t fps_den)
{
	if (!view)
		return;
	auto *h = headerOf(view);
	h->fpsNum = fps_num;
	h->fpsDen = fps_den;
}

uint8_t *PushFpsOutput::beginFrame(uint32_t width, uint32_t height)
{
	if (!view || pending_slot >= 0 || !width || !height || width > kMaxWidth || height > kMaxHeight)
		return nullptr;
	auto *h = headerOf(view);
	const uint32_t index = (h->latestSlot + 1) % PUSHFPS_FRAME_INPUT_SLOTS;
	InterlockedIncrement(reinterpret_cast<volatile LONG *>(&h->slots[index].seq)); // odd: writing
	pending_slot = static_cast<int>(index);
	pending_width = width;
	pending_height = height;
	return view + PUSHFPS_FRAME_INPUT_HEADER_BYTES + static_cast<uint64_t>(index) * h->slotBytes;
}

void PushFpsOutput::commitFrame(int64_t arrival_100ns, bool discontinuity, int64_t content_100ns)
{
	if (!view || pending_slot < 0)
		return;
	auto *h = headerOf(view);
	auto &slot = h->slots[pending_slot];
	slot.width = pending_width;
	slot.height = pending_height;
	slot.stride = pending_width * 4u;
	slot.format = PUSHFPS_FORMAT_RGBA8;
	slot.flags = discontinuity ? 1u : 0u;
	const int64_t arrival = arrival_100ns ? arrival_100ns : nowTimestamp100ns();
	const int64_t interval = (h->fpsNum > 0 && h->fpsDen > 0)
		? (10000000LL * static_cast<int64_t>(h->fpsDen)) / static_cast<int64_t>(h->fpsNum) : 0;
	if (discontinuity)
		have_timestamp = false;
	// The stream clock's own step when it is known and plausible (it already counts lost frames)
	int64_t step = interval;
	if (content_100ns >= 0 && last_content >= 0 && content_100ns > last_content && content_100ns - last_content <= 10000000)
		step = content_100ns - last_content;
	last_content = content_100ns;
	slot.timestamp100ns = smoothTimestamp(arrival, step, interval, last_timestamp, have_timestamp);
	slot.frameIndex = ++sent;
	InterlockedIncrement(reinterpret_cast<volatile LONG *>(&slot.seq)); // even: complete
	InterlockedExchange(reinterpret_cast<volatile LONG *>(&h->latestSlot), static_cast<LONG>(pending_slot));
	InterlockedIncrement64(reinterpret_cast<volatile LONG64 *>(&h->frameCounter));
	SetEvent(static_cast<HANDLE>(ready));
	pending_slot = -1;
}

void PushFpsOutput::cancelFrame()
{
	if (!view || pending_slot < 0)
		return;
	auto *h = headerOf(view);
	InterlockedIncrement(reinterpret_cast<volatile LONG *>(&h->slots[pending_slot].seq)); // even again
	pending_slot = -1;
}

#else // !_WIN32

PushFpsOutput::~PushFpsOutput() {}
int64_t PushFpsOutput::nowTimestamp100ns() { return 0; }
bool PushFpsOutput::open(uint32_t, uint32_t, const char *, const wchar_t *) { return false; }
void PushFpsOutput::close() {}
void PushFpsOutput::setFps(uint32_t, uint32_t) {}
uint8_t *PushFpsOutput::beginFrame(uint32_t, uint32_t) { return nullptr; }
void PushFpsOutput::commitFrame(int64_t, bool, int64_t) {}
void PushFpsOutput::cancelFrame() {}

#endif
