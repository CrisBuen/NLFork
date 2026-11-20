// NLFork: sends the rendered stream picture to PushFPS through its Frame Input API
// (shared memory, see include/pushfps/FrameInput.h). PushFPS then treats NLFork like a capture card:
// frame generation, AI enhancement and recording run on NLFork's own picture, with NLFork's
// render settings, controller and session. Windows only; elsewhere every call is a no-op.
#ifndef NLFORK_PUSHFPSOUTPUT_H
#define NLFORK_PUSHFPSOUTPUT_H

#include <cstdint>

class PushFpsOutput
{
public:
	// Largest picture the channel takes. 4K keeps every output size possible without reopening
	// the channel (PushFPS keeps the mapping open, so its size can't grow later).
	static constexpr uint32_t kMaxWidth = 3840;
	static constexpr uint32_t kMaxHeight = 2160;

	PushFpsOutput() = default;
	~PushFpsOutput();
	PushFpsOutput(const PushFpsOutput &) = delete;
	PushFpsOutput &operator=(const PushFpsOutput &) = delete;

	// Creates the channel "Local\PushFPS.FrameInput.<channel>" ("default" is the one PushFPS lists
	// first). fps is the stream's nominal rate.
	bool open(uint32_t fps_num, uint32_t fps_den, const char *sender_name, const wchar_t *channel = L"default");
	void close();
	bool isOpen() const { return view != nullptr; }
	void setFps(uint32_t fps_num, uint32_t fps_den);

	// Starts writing the next slot and returns where its pixels go (RGBA8, rows width * 4 bytes
	// apart), or nullptr. Every beginFrame() is followed by commitFrame() or cancelFrame().
	uint8_t *beginFrame(uint32_t width, uint32_t height);
	// Publishes the slot. arrival_100ns: when the picture became ready (QueryPerformanceCounter in
	// 100 ns units, 0 = now); content_100ns: its time on the stream's own clock (the decoder's
	// steady timeline, which counts lost frames), or -1 when unknown. The timestamp PushFPS gets
	// is the arrival evened out along that clock (see smoothTimestamp), because PushFPS paces
	// frame generation from the timestamps.
	void commitFrame(int64_t arrival_100ns, bool discontinuity, int64_t content_100ns = -1);
	// Leaves the slot unpublished (its pixels may be half written; PushFPS never reads it).
	void cancelFrame();

	uint64_t framesSent() const { return sent; }
	static int64_t nowTimestamp100ns();

	// Arrival-disciplined timestamp, like PushFPS's own Remote Play clock: frames keep the stream's
	// rhythm (last + step) when they arrive late, take their arrival when on time or early (a
	// timestamp is never later than the arrival), and start over after a long gap. `step` is the
	// distance on the stream's clock since the last frame (nominal interval when unknown);
	// `interval` is the nominal frame interval.
	static int64_t smoothTimestamp(int64_t arrival, int64_t step, int64_t interval, int64_t &last, bool &have_last);

private:
	int64_t last_timestamp = 0;
	int64_t last_content = -1;
	bool have_timestamp = false;
	void *mapping = nullptr;
	void *ready = nullptr;
	uint8_t *view = nullptr;
	uint64_t sent = 0;
	int pending_slot = -1;
	uint32_t pending_width = 0;
	uint32_t pending_height = 0;
};

#endif // NLFORK_PUSHFPSOUTPUT_H
