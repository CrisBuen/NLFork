// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <controllermanager.h>

#include <QCoreApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

/* PS5 trigger effect documentation:
   https://controllers.fandom.com/wiki/Sony_DualSense#FFB_Trigger_Modes

   Taken from SDL2, licensed under the zlib license,
   Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
   https://github.com/libsdl-org/SDL/blob/release-2.24.1/test/testgamecontroller.c#L263-L289
*/
typedef struct
{
    Uint8 ucEnableBits1;                /* 0 */
    Uint8 ucEnableBits2;                /* 1 */
    Uint8 ucRumbleRight;                /* 2 */
    Uint8 ucRumbleLeft;                 /* 3 */
    Uint8 ucHeadphoneVolume;            /* 4 */
    Uint8 ucSpeakerVolume;              /* 5 */
    Uint8 ucMicrophoneVolume;           /* 6 */
    Uint8 ucAudioEnableBits;            /* 7 */
    Uint8 ucMicLightMode;               /* 8 */
    Uint8 ucAudioMuteBits;              /* 9 */
    Uint8 rgucRightTriggerEffect[11];   /* 10 */
    Uint8 rgucLeftTriggerEffect[11];    /* 21 */
    Uint8 rgucUnknown1[6];              /* 32 */
    Uint8 ucEnableBits3;                /* 38 */
    Uint8 rgucUnknown2[2];              /* 39 */
    Uint8 ucLedAnim;                    /* 41 */
    Uint8 ucLedBrightness;              /* 42 */
    Uint8 ucPadLights;                  /* 43 */
    Uint8 ucLedRed;                     /* 44 */
    Uint8 ucLedGreen;                   /* 45 */
    Uint8 ucLedBlue;                    /* 46 */
} DS5EffectsState_t;

static QSet<QString> chiaki_motion_controller_guids({
	// Sony on Linux
	"03000000341a00003608000011010000",
	"030000004c0500006802000010010000",
	"030000004c0500006802000010810000",
	"030000004c0500006802000011010000",
	"030000004c0500006802000011810000",
	"030000006f0e00001402000011010000",
	"030000008f0e00000300000010010000",
	"050000004c0500006802000000010000",
	"050000004c0500006802000000800000",
	"050000004c0500006802000000810000",
	"05000000504c415953544154494f4e00",
	"060000004c0500006802000000010000",
	"030000004c050000a00b000011010000",
	"030000004c050000a00b000011810000",
	"030000004c050000c405000011010000",
	"030000004c050000c405000011810000",
	"030000004c050000cc09000000010000",
	"030000004c050000cc09000011010000",
	"030000004c050000cc09000011810000",
	"03000000c01100000140000011010000",
	"050000004c050000c405000000010000",
	"050000004c050000c405000000810000",
	"050000004c050000c405000001800000",
	"050000004c050000cc09000000010000",
	"050000004c050000cc09000000810000",
	"050000004c050000cc09000001800000",
	// Sony on iOS
	"050000004c050000cc090000df070000",
	// Sony on Android
	"050000004c05000068020000dfff3f00",
	"030000004c050000cc09000000006800",
	"050000004c050000c4050000fffe3f00",
	"050000004c050000cc090000fffe3f00",
	"050000004c050000cc090000ffff3f00",
	"35643031303033326130316330353564",
	// Sony on Mac OSx
	"030000004c050000cc09000000000000",
	"030000004c0500006802000000000000",
	"030000004c0500006802000000010000",
	"030000004c050000a00b000000010000",
	"030000004c050000c405000000000000",
	"030000004c050000c405000000010000",
	"030000004c050000cc09000000010000",
	"03000000c01100000140000000010000",
	// Sony on Windows
	"030000004c050000a00b000000000000",
	"030000004c050000c405000000000000",
	"030000004c050000cc09000000000000",
	"03000000250900000500000000000000",
	"030000004c0500006802000000000000",
	"03000000632500007505000000000000",
	"03000000888800000803000000000000",
	"030000008f0e00001431000000000000",
});

static QSet<QPair<uint16_t, uint16_t>> chiaki_dualsense_controller_ids({
	// in format (vendor id, product id)
	QPair<uint16_t, uint16_t>(0x054c, 0x0ce6), // DualSense controller
});

static QSet<QPair<uint16_t, uint16_t>> chiaki_dualsense_edge_controller_ids({
	// in format (vendor id, product id)
	QPair<uint16_t, uint16_t>(0x054c, 0x0df2), // DualSense Edge controller
});

static QSet<QPair<uint16_t, uint16_t>> chiaki_handheld_controller_ids({
	// in format (vendor id, product id)
	QPair<uint16_t, uint16_t>(0x28de, 0x1205), // Steam Deck
	QPair<uint16_t, uint16_t>(0x0b05, 0x1abe), // Rog Ally
	QPair<uint16_t, uint16_t>(0x17ef, 0x6182), // Legion Go
	QPair<uint16_t, uint16_t>(0x0db0, 0x1901), // MSI Claw
});

static QSet<QPair<uint16_t, uint16_t>> chiaki_steam_virtual_controller_ids({
	// in format (vendor id, product id)
#ifdef Q_OS_MACOS
    QPair<uint16_t, uint16_t>(0x045e, 0x028e), // Microsoft Xbox 360 Controller
#else
	QPair<uint16_t, uint16_t>(0x28de, 0x11ff), // Steam Virtual Controller
#endif
});

static ControllerManager *instance = nullptr;

// A DualSense on USB reports every 4 ms (measured: 250 Hz). Polling every 1 ms instead of every
// 4 ms means a new report waits ~0.7 ms on average before it is read, instead of ~2 ms.
#define UPDATE_INTERVAL_MS 1
#define MOVE_CHECK_MS 1000
// Hitch detector: controller input is read on the GUI thread, so if that thread is blocked the
// input reaches the console late. Normal gaps are ~1-2 ms; only log while a controller is in use.
#define CONTROLLER_POLL_HITCH_MS 25

ControllerManager *ControllerManager::GetInstance()
{
	if(!instance)
		instance = new ControllerManager(qApp);
	return instance;
}

ControllerManager::ControllerManager(QObject *parent)
	: QObject(parent), creating_controller_mapping(false),
	joystick_allow_background_events(true), dualsense_intensity(0x00), is_app_active(true)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_SetMainReady();
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#if SDL_VERSION_ATLEAST(2, 29, 1)
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "0");
#endif
	if(SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
		return;

	auto timer = new QTimer(this);
	timer->setTimerType(Qt::PreciseTimer);
	connect(timer, &QTimer::timeout, this, &ControllerManager::HandleEvents);
	timer->start(UPDATE_INTERVAL_MS);
	auto move_timer = new QTimer(this);
	connect(move_timer, &QTimer::timeout, this, &ControllerManager::CheckMoved);
	move_timer->start(MOVE_CHECK_MS);
#endif

	SetButtonRemap(QString());
	UpdateAvailableControllers();
}

ControllerManager::~ControllerManager()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	open_controllers.clear();
	SDL_Quit();
#endif
}

void ControllerManager::SetAllowJoystickBackgroundEvents(bool enabled)
{
	this->joystick_allow_background_events = enabled;
}

void ControllerManager::CheckMoved()
{
	if(this->moved)
	{
		this->moved = false;
		emit ControllerMoved();
	}
}

void ControllerManager::SetIsAppActive(bool active)
{
	this->is_app_active = active;
}

void ControllerManager::SetButtonsByPos()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#endif
}

// ---------------------------------------------------------------------------------------------
// NLFork button remapping
// ---------------------------------------------------------------------------------------------

// A button that is part of a combo waits this long before acting alone, so pressing the first
// button of a combo does not also send its own action to the console.
#define REMAP_COMBO_GRACE_MS 70
// How long a quick tap of such a button is held down for the console.
#define REMAP_TAP_MS 60

struct RemapActionInfo
{
	const char *action;
	uint32_t ps;
	const char *default_binding;
};

static const RemapActionInfo remap_action_info[] = {
	{"cross", CHIAKI_CONTROLLER_BUTTON_CROSS, "a"},
	{"circle", CHIAKI_CONTROLLER_BUTTON_MOON, "b"},
	{"square", CHIAKI_CONTROLLER_BUTTON_BOX, "x"},
	{"triangle", CHIAKI_CONTROLLER_BUTTON_PYRAMID, "y"},
	{"up", CHIAKI_CONTROLLER_BUTTON_DPAD_UP, "dpup"},
	{"down", CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN, "dpdown"},
	{"left", CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT, "dpleft"},
	{"right", CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT, "dpright"},
	{"l1", CHIAKI_CONTROLLER_BUTTON_L1, "leftshoulder"},
	{"r1", CHIAKI_CONTROLLER_BUTTON_R1, "rightshoulder"},
	{"l2", CHIAKI_CONTROLLER_ANALOG_BUTTON_L2, ""},
	{"r2", CHIAKI_CONTROLLER_ANALOG_BUTTON_R2, ""},
	{"l3", CHIAKI_CONTROLLER_BUTTON_L3, "leftstick"},
	{"r3", CHIAKI_CONTROLLER_BUTTON_R3, "rightstick"},
	{"options", CHIAKI_CONTROLLER_BUTTON_OPTIONS, "start"},
	{"create", CHIAKI_CONTROLLER_BUTTON_SHARE, "back"},
	{"touchpad", CHIAKI_CONTROLLER_BUTTON_TOUCHPAD, "touchpad"},
	{"ps", CHIAKI_CONTROLLER_BUTTON_PS, "guide"},
};

static int PopCount(uint32_t v)
{
	int c = 0;
	for(; v; v &= v - 1)
		c++;
	return c;
}

// "back+start" -> bit mask of SDL buttons (0 if empty or invalid)
static uint32_t RemapBindingMask(const QString &binding)
{
	uint32_t mask = 0;
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	const auto parts = binding.split('+', Qt::SkipEmptyParts);
	for(const QString &part : parts)
	{
		const SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(part.trimmed().toUtf8().constData());
		if(b == SDL_CONTROLLER_BUTTON_INVALID || b >= 32)
			return 0;
		mask |= 1u << b;
	}
	if(PopCount(mask) > 2)
		return 0;
#else
	Q_UNUSED(binding);
#endif
	return mask;
}

QStringList ControllerManager::RemapActions()
{
	QStringList list;
	for(const auto &info : remap_action_info)
		list.append(info.action);
	return list;
}

QString ControllerManager::DefaultRemapBinding(const QString &action)
{
	for(const auto &info : remap_action_info)
		if(action == info.action)
			return info.default_binding;
	return QString();
}

QString ControllerManager::RemapBindingLabel(const QString &binding)
{
	static const QMap<QString, QString> names = {
		{"a", "A"}, {"b", "B"}, {"x", "X"}, {"y", "Y"},
		{"back", "View / Select"}, {"guide", "Home"}, {"start", "Menu / Start"},
		{"leftstick", "LS (L3)"}, {"rightstick", "RS (R3)"},
		{"leftshoulder", "LB (L1)"}, {"rightshoulder", "RB (R1)"},
		{"dpup", "D-pad ↑"}, {"dpdown", "D-pad ↓"}, {"dpleft", "D-pad ←"}, {"dpright", "D-pad →"},
		{"misc1", "Share / Capture"},
		{"paddle1", "P1"}, {"paddle2", "P2"}, {"paddle3", "P3"}, {"paddle4", "P4"},
		{"touchpad", "Touchpad"},
	};
	if(binding.isEmpty())
		return QString();
	QStringList out;
	for(const QString &part : binding.split('+', Qt::SkipEmptyParts))
		out.append(names.value(part, part));
	return out.join(" + ");
}

// Keeps only known actions with valid bindings; actions equal to their default are dropped.
QString ControllerManager::NormalizeRemapSpec(const QString &spec)
{
	QMap<QString, QString> values;
	for(const QString &item : spec.split(';', Qt::SkipEmptyParts))
	{
		const int eq = item.indexOf('=');
		if(eq <= 0)
			continue;
		const QString action = item.left(eq).trimmed();
		const QString binding = item.mid(eq + 1).trimmed();
		if(!RemapActions().contains(action))
			continue;
		if(!binding.isEmpty() && !RemapBindingMask(binding))
			continue;
		if(binding == DefaultRemapBinding(action))
			continue;
		values[action] = binding;
	}
	QStringList out;
	for(const QString &action : RemapActions())
		if(values.contains(action))
			out.append(action + "=" + values[action]);
	return out.join(';');
}

void ControllerManager::SetButtonRemap(const QString &spec)
{
	QMap<QString, QString> custom;
	for(const QString &item : NormalizeRemapSpec(spec).split(';', Qt::SkipEmptyParts))
	{
		const int eq = item.indexOf('=');
		custom[item.left(eq)] = item.mid(eq + 1);
	}

	button_remap.clear();
	remap_combo_bits = 0;
	remap_used_bits = 0;
	for(const auto &info : remap_action_info)
	{
		const QString binding = custom.contains(info.action) ? custom[info.action] : QString(info.default_binding);
		const uint32_t mask = RemapBindingMask(binding);
		if(!mask)
			continue;
		button_remap.append({info.action, info.ps, mask});
		remap_used_bits |= mask;
		if(PopCount(mask) >= 2)
			remap_combo_bits |= mask;
	}
	for(Controller *controller : std::as_const(open_controllers))
		controller->NotifyRemapChanged();
}

QString ControllerManager::RemapBinding(const QString &action) const
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	for(const auto &entry : button_remap)
	{
		if(entry.action != action)
			continue;
		QStringList parts;
		for(int b = 0; b < 32; b++)
			if(entry.mask & (1u << b))
				parts.append(SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(b)));
		return parts.join('+');
	}
#else
	Q_UNUSED(action);
#endif
	return QString();
}

bool ControllerManager::RemapBindingAvailable(const QString &binding) const
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	const uint32_t mask = RemapBindingMask(binding);
	if(!mask || open_controllers.isEmpty())
		return true;
	for(Controller *controller : std::as_const(open_controllers))
	{
		SDL_GameController *gc = controller->GetController();
		if(!gc)
			continue;
		bool all = true;
		for(int b = 0; b < 32; b++)
			if((mask & (1u << b)) && !SDL_GameControllerHasButton(gc, static_cast<SDL_GameControllerButton>(b)))
				all = false;
		if(all)
			return true;
	}
	return false;
#else
	Q_UNUSED(binding);
	return true;
#endif
}

void ControllerManager::StartButtonCapture()
{
	button_capture = true;
	capture_held = 0;
	capture_mask = 0;
}

void ControllerManager::CancelButtonCapture()
{
	button_capture = false;
	capture_held = 0;
	capture_mask = 0;
}

void ControllerManager::CaptureButton(uint32_t bit, bool down)
{
	if(down)
	{
		if(PopCount(capture_mask) >= 2 && !(capture_mask & bit))
			return; // combos have at most two buttons
		capture_held |= bit;
		capture_mask |= bit;
		return;
	}
	if(!(capture_mask & bit))
		return; // e.g. releasing the button that opened the capture
	capture_held &= ~bit;
	if(capture_held)
		return;
	QStringList parts;
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	for(int b = 0; b < 32; b++)
		if(capture_mask & (1u << b))
			parts.append(SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(b)));
#endif
	button_capture = false;
	capture_mask = 0;
	emit ButtonCaptured(parts.join('+'));
}

void ControllerManager::UpdateAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	QSet<SDL_JoystickID> current_controllers;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(!SDL_IsGameController(i))
			continue;

		// We'll try to identify pads with Motion Control
		SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_JoystickOpen(i));
		char guid_str[256];
		SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

		if(chiaki_motion_controller_guids.contains(guid_str))
		{
			SDL_Joystick *joy = SDL_JoystickOpen(i);
			if(joy)
			{
				bool no_buttons = SDL_JoystickNumButtons(joy) == 0;
				SDL_JoystickClose(joy);
				if(no_buttons)
					continue;
			}
		}

		current_controllers.insert(SDL_JoystickGetDeviceInstanceID(i));
	}

	if(current_controllers != available_controllers)
	{
		available_controllers = std::move(current_controllers);
		emit AvailableControllersUpdated();
	}
#endif
}

void ControllerManager::creatingControllerMapping(bool creating_controller_mapping)
{
	this->creating_controller_mapping = creating_controller_mapping;
}

void ControllerManager::HandleEvents()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	static QElapsedTimer poll_clock;
	static qint64 last_poll_ms = -1;
	if(!poll_clock.isValid())
		poll_clock.start();
	const qint64 now_ms = poll_clock.elapsed();
	if(last_poll_ms >= 0 && !open_controllers.isEmpty() && now_ms - last_poll_ms >= CONTROLLER_POLL_HITCH_MS)
	{
		poll_hitch_count++;
		qCWarning(chiakiGui).nospace() << "[hitch] Controller input was not read for " << (now_ms - last_poll_ms)
			<< " ms (GUI thread busy, input reached the console late)";
	}
	last_poll_ms = now_ms;

	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				UpdateAvailableControllers();
				break;
			case SDL_AUDIODEVICEADDED:
				// a DualSense's haptics audio device can appear well after the controller
				if(!event.adevice.iscapture)
				{
					const char *name = SDL_GetAudioDeviceName(static_cast<int>(event.adevice.which), 0);
					emit AudioOutputDeviceAdded(name ? QString::fromUtf8(name) : QString());
				}
				break;
			case SDL_JOYAXISMOTION:
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_JOYHATMOTION:
			case SDL_CONTROLLERBUTTONUP:
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERAXISMOTION:
#if not defined(CHIAKI_ENABLE_SETSU) and SDL_VERSION_ATLEAST(2, 0, 14)
			case SDL_CONTROLLERSENSORUPDATE:
			case SDL_CONTROLLERTOUCHPADDOWN:
			case SDL_CONTROLLERTOUCHPADMOTION:
			case SDL_CONTROLLERTOUCHPADUP:
#endif
				if(joystick_allow_background_events || is_app_active)
					ControllerEvent(event);
				break;
		}
	}
#endif
}

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
void ControllerManager::ControllerEvent(SDL_Event event)
{
	int device_id;
	bool start_updating_mapping = false;
	switch(event.type)
	{
		case SDL_CONTROLLERBUTTONDOWN:
			device_id = event.cbutton.which;
			break;
		case SDL_CONTROLLERBUTTONUP:
			device_id = event.cbutton.which;
			break;
		case SDL_CONTROLLERAXISMOTION:
			device_id = event.caxis.which;
			break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
		case SDL_CONTROLLERSENSORUPDATE:
			device_id = event.csensor.which;
			break;
		case SDL_CONTROLLERTOUCHPADDOWN:
			device_id = event.ctouchpad.which;
			break;
		case SDL_CONTROLLERTOUCHPADMOTION:
		case SDL_CONTROLLERTOUCHPADUP:
			device_id = event.ctouchpad.which;
			break;
#endif
		case SDL_JOYAXISMOTION:
			device_id = event.jaxis.which;
			break;
		case SDL_JOYBUTTONDOWN:
			start_updating_mapping = true;
			device_id = event.jbutton.which;
			break;
		case SDL_JOYBUTTONUP:
			device_id = event.jbutton.which;
			break;
		case SDL_JOYHATMOTION:
			start_updating_mapping = true;
			device_id = event.jhat.which;
			break;
		default:
			return;
	}
	if(!open_controllers.contains(device_id))
		return;
	if(creating_controller_mapping && start_updating_mapping)
	{
		open_controllers[device_id]->StartUpdatingMapping();
		creating_controller_mapping = false;
	}
	open_controllers[device_id]->UpdateState(event);
}
#endif

QSet<int> ControllerManager::GetAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return available_controllers;
#else
	return {};
#endif
}

Controller *ControllerManager::OpenController(int device_id)
{
	Controller *controller = open_controllers.value(device_id);
	if(!controller)
	{
		controller = new Controller(device_id, this);
		open_controllers[device_id] = controller;
	}
	controller->Ref();
	return controller;
}

void ControllerManager::ControllerClosed(Controller *controller)
{
	open_controllers.remove(controller->GetDeviceID());
}

Controller::Controller(int device_id, ControllerManager *manager)
: QObject(manager), ref(0), last_motion_timestamp(0), micbutton_push(false), is_dualsense(false),
  is_dualsense_edge(false), has_led(false), firmware_version(0), updating_mapping_button(false), is_handheld(false),
  is_steam_virtual(false), is_steam_virtual_unmasked(false), enable_analog_stick_mapping(false)
{
	this->id = device_id;
	this->manager = manager;
	chiaki_orientation_tracker_init(&this->orientation_tracker);
	chiaki_accel_new_zero_set_inactive(&this->accel_zero, false);
	chiaki_accel_new_zero_set_inactive(&this->real_accel, true);
	chiaki_controller_state_set_idle(&this->state);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	controller = nullptr;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(SDL_JoystickGetDeviceInstanceID(i) == device_id)
		{
			controller = SDL_GameControllerOpen(i);
#if SDL_VERSION_ATLEAST(2, 0, 14)
			if(SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL))
				SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
			if(SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO))
				SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
#endif
			has_led = SDL_GameControllerHasLED(controller);
			auto controller_id = QPair<uint16_t, uint16_t>(SDL_GameControllerGetVendor(controller), SDL_GameControllerGetProduct(controller));
			is_dualsense = chiaki_dualsense_controller_ids.contains(controller_id);
			is_handheld = chiaki_handheld_controller_ids.contains(controller_id);
			is_dualsense_edge = chiaki_dualsense_edge_controller_ids.contains(controller_id);
			firmware_version = SDL_GameControllerGetFirmwareVersion(controller);
			SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
			SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
			auto guid_controller_id = QPair<uint16_t, uint16_t>(0, 0);
			uint16_t guid_version = 0;
			SDL_GetJoystickGUIDInfo(guid, &guid_controller_id.first, &guid_controller_id.second, &guid_version, NULL);
#ifdef Q_OS_MACOS
			is_steam_virtual = (guid_version == 0 && chiaki_steam_virtual_controller_ids.contains(guid_controller_id));
			is_steam_virtual_unmasked = (guid_version == 0 && chiaki_steam_virtual_controller_ids.contains(controller_id));
#else
			is_steam_virtual = chiaki_steam_virtual_controller_ids.contains(guid_controller_id);
			is_steam_virtual_unmasked = chiaki_steam_virtual_controller_ids.contains(controller_id);
#endif
			break;
		}
	}
#endif
}

Controller::~Controller()
{
	Q_ASSERT(ref == 0);
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(controller && SDL_WasInit(SDL_INIT_GAMECONTROLLER)!=0)
	{
		// Clear trigger effects, SDL doesn't do it automatically
		const uint8_t clear_effect[10] = { 0 };
		this->SetTriggerEffects(0x05, clear_effect, 0x05, clear_effect);
		this->SetRumble(0,0);
		SDL_GameControllerClose(controller);
	}
#endif
}

void Controller::StartUpdatingMapping()
{
	emit UpdatingControllerMapping(this);
}

void Controller::IsUpdatingMappingButton(bool is_updating_mapping_button)
{
	this->updating_mapping_button = is_updating_mapping_button;
}

void Controller::EnableAnalogStickMapping(bool enabled)
{
	this->enable_analog_stick_mapping = enabled;
}

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
void Controller::UpdateState(SDL_Event event)
{
	switch(event.type)
	{
		case SDL_JOYAXISMOTION:
			if(updating_mapping_button && enable_analog_stick_mapping)
			{
				emit NewButtonMapping(QString("a%1").arg(QString::number(event.jaxis.axis)));
				updating_mapping_button = false;
			}
			return;
		case SDL_JOYBUTTONDOWN:
			if(updating_mapping_button)
			{
				emit NewButtonMapping(QString("b%1").arg(QString::number(event.jbutton.button)));
				updating_mapping_button = false;
			}
			return;
		case SDL_JOYBUTTONUP:
			return;
		case SDL_JOYHATMOTION:
			if(updating_mapping_button)
			{
				emit NewButtonMapping(QString("h%1.%2").arg(QString::number(event.jhat.hat)).arg(QString::number(event.jhat.value)));
				updating_mapping_button = false;
			}
			return;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			if(!HandleButtonEvent(event.cbutton))
				return;
			break;
		case SDL_CONTROLLERAXISMOTION:
			if(!HandleAxisEvent(event.caxis))
				return;
			break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
		case SDL_CONTROLLERSENSORUPDATE:
			if(!HandleSensorEvent(event.csensor))
				return;
			break;
		case SDL_CONTROLLERTOUCHPADDOWN:
		case SDL_CONTROLLERTOUCHPADMOTION:
		case SDL_CONTROLLERTOUCHPADUP:
			if(!HandleTouchpadEvent(event.ctouchpad))
				return;
			break;
#endif
		default:
			return;

	}
	emit StateChanged();
	manager->moved = true;
}

// Buttons go through the user's remap table (ControllerManager::button_remap). The default table
// is the classic layout: A = Cross, B = Circle, View = Create, Menu = Options, and so on.
inline bool Controller::HandleButtonEvent(SDL_ControllerButtonEvent event) {
	if(event.button >= 32)
		return false;
	const uint32_t bit = 1u << event.button;
	const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;

	if(manager->button_capture)
	{
		// While the Controller dialog listens for a new binding nothing new reaches the console
		// or the menus; releases still go through so no button stays stuck.
		if(down)
		{
			manager->CaptureButton(bit, true);
			return false;
		}
		manager->CaptureButton(bit, false);
		held_sdl &= ~bit;
		grace_bits &= ~bit;
		combo_locked &= ~bit;
		RecomputeButtons();
		return true;
	}

	// Without a binding the Share/Capture button keeps toggling the microphone
	if(event.button == SDL_CONTROLLER_BUTTON_MISC1 && !(manager->remap_used_bits & bit))
	{
		if(!down)
			emit MicButtonPush();
		return false;
	}

	if(down)
	{
		held_sdl |= bit;
		bool completed_combo = false;
		for(const auto &entry : std::as_const(manager->button_remap))
		{
			if(PopCount(entry.mask) >= 2 && (entry.mask & bit) && (held_sdl & entry.mask) == entry.mask)
			{
				combo_locked |= entry.mask;
				grace_bits &= ~entry.mask;
				completed_combo = true;
			}
		}
		if(!completed_combo && (manager->remap_combo_bits & bit))
		{
			grace_bits |= bit;
			QTimer::singleShot(REMAP_COMBO_GRACE_MS, this, [this, bit]() {
				if(!(grace_bits & bit))
					return;
				grace_bits &= ~bit;
				RecomputeButtons();
				emit StateChanged();
			});
		}
	}
	else
	{
		held_sdl &= ~bit;
		combo_locked &= ~bit;
		if(grace_bits & bit)
		{
			// Released before the grace time ended: send it as a short tap
			grace_bits &= ~bit;
			const uint32_t tap = SingleActionFor(bit);
			if(tap)
			{
				tap_ps |= tap;
				QTimer::singleShot(REMAP_TAP_MS, this, [this, tap]() {
					tap_ps &= ~tap;
					RecomputeButtons();
					emit StateChanged();
				});
			}
		}
	}
	RecomputeButtons();
	return true;
}

uint32_t Controller::SingleActionFor(uint32_t bit) const
{
	uint32_t ps = 0;
	for(const auto &entry : std::as_const(manager->button_remap))
		if(entry.mask == bit)
			ps |= entry.ps;
	return ps;
}

void Controller::RecomputeButtons()
{
	uint32_t ps = 0;
	uint32_t consumed = 0;
	const auto &remap = manager->button_remap;
	for(const auto &entry : remap)
	{
		if(PopCount(entry.mask) >= 2 && (held_sdl & entry.mask) == entry.mask)
		{
			ps |= entry.ps;
			consumed |= entry.mask;
		}
	}
	const uint32_t silent = consumed | combo_locked | grace_bits;
	for(const auto &entry : remap)
	{
		if(PopCount(entry.mask) == 1 && (held_sdl & entry.mask) && !(silent & entry.mask))
			ps |= entry.ps;
	}
	ps |= tap_ps;
	state.buttons = ps & 0xffff;
	state.l2_state = (ps & CHIAKI_CONTROLLER_ANALOG_BUTTON_L2) ? 0xff : analog_l2;
	state.r2_state = (ps & CHIAKI_CONTROLLER_ANALOG_BUTTON_R2) ? 0xff : analog_r2;
}

void Controller::NotifyRemapChanged()
{
	grace_bits = 0;
	combo_locked = 0;
	tap_ps = 0;
	RecomputeButtons();
	emit StateChanged();
}

inline bool Controller::HandleAxisEvent(SDL_ControllerAxisEvent event) {
	switch(event.axis)
	{
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			analog_l2 = (uint8_t)(event.value >> 7);
			RecomputeButtons();
			break;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
			analog_r2 = (uint8_t)(event.value >> 7);
			RecomputeButtons();
			break;
		case SDL_CONTROLLER_AXIS_LEFTX:
			state.left_x = event.value;
			break;
		case SDL_CONTROLLER_AXIS_LEFTY:
			state.left_y = event.value;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTX:
			state.right_x = event.value;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTY:
			state.right_y = event.value;
			break;
		default:
			return false;
	}
	return true;
}

#if SDL_VERSION_ATLEAST(2, 0, 14)
inline bool Controller::HandleSensorEvent(SDL_ControllerSensorEvent event)
{
	float accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z;
	switch(event.sensor)
	{
		case SDL_SENSOR_ACCEL:
			accel_x = event.data[0] / SDL_STANDARD_GRAVITY;
			accel_y = event.data[1] / SDL_STANDARD_GRAVITY;
			accel_z = event.data[2] / SDL_STANDARD_GRAVITY;
			chiaki_accel_new_zero_set_active(&this->real_accel,
			accel_x, accel_y, accel_z, true);
			chiaki_orientation_tracker_update(
				&orientation_tracker, state.gyro_x, state.gyro_y, state.gyro_z,
				accel_x, accel_y, accel_z, &accel_zero, false, event.timestamp * 1000);
			break;
		case SDL_SENSOR_GYRO:
			gyro_x = event.data[0];
			gyro_y = event.data[1];
			gyro_z = event.data[2];
			chiaki_orientation_tracker_update(
				&orientation_tracker, gyro_x, gyro_y, gyro_z,
				state.accel_x, state.accel_y, state.accel_z, &accel_zero, true, event.timestamp * 1000);
			break;
		default:
			return false;
	}
	last_motion_timestamp = event.timestamp * 1000;
	chiaki_orientation_tracker_apply_to_controller_state(&orientation_tracker, &state);
	return true;
}

inline bool Controller::HandleTouchpadEvent(SDL_ControllerTouchpadEvent event)
{
	auto key = qMakePair(event.touchpad, event.finger);
	bool exists = touch_ids.contains(key);
	uint8_t chiaki_id;
	switch(event.type)
	{
		case SDL_CONTROLLERTOUCHPADDOWN:
			if(touch_ids.size() >= CHIAKI_CONTROLLER_TOUCHES_MAX)
				return false;
			chiaki_id = chiaki_controller_state_start_touch(&state, event.x * PS_TOUCHPAD_MAXX, event.y * PS_TOUCHPAD_MAXY);
			touch_ids.insert(key, chiaki_id);
			break;
		case SDL_CONTROLLERTOUCHPADMOTION:
			if(!exists)
				return false;
			chiaki_controller_state_set_touch_pos(&state, touch_ids[key], event.x * PS_TOUCHPAD_MAXX, event.y * PS_TOUCHPAD_MAXY);
			break;
		case SDL_CONTROLLERTOUCHPADUP:
			if(!exists)
				return false;
			chiaki_controller_state_stop_touch(&state, touch_ids[key]);
			touch_ids.remove(key);
			break;
		default:
			return false;
	}
	return true;
}
#endif
#endif

void Controller::Ref()
{
	ref++;
}

void Controller::Unref()
{
	if(--ref == 0)
	{
		manager->ControllerClosed(this);
		deleteLater();
	}
}

bool Controller::IsConnected()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return controller && SDL_GameControllerGetAttached(controller);
#else
	return false;
#endif
}

int Controller::GetDeviceID()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return id;
#else
	return -1;
#endif
}

QString Controller::GetType()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
	return QString("%1").arg(SDL_JoystickName(js));
#else
	return QString();
#endif
}

bool Controller::IsPS()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	SDL_GameControllerType type = SDL_GameControllerGetType(controller);
	if(type == SDL_CONTROLLER_TYPE_PS3 || type == SDL_CONTROLLER_TYPE_PS4 || type == SDL_CONTROLLER_TYPE_PS5)
		return true;
	else
		return false;
#else
	return false;
#endif
}

QString Controller::GetGUIDString()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
	char guid_str[256];
	SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
	SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
	return QString("%1").arg(guid_str);
#else
	return QString();
#endif
}

QString Controller::GetName()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
	SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
	char guid_str[256];
	SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
	return QString("%1 (%2)").arg(SDL_JoystickName(js), guid_str);
#else
	return QString();
#endif
}

QString Controller::GetVIDPIDString()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	QString vid_pid = QString("0x%1:0x%2").arg(SDL_GameControllerGetVendor(controller), 4, 16, QChar('0')).arg(SDL_GameControllerGetProduct(controller), 4, 16, QChar('0'));
	return vid_pid;
#else
	return QString();
#endif
}

ChiakiControllerState Controller::GetState()
{
	return state;
}

bool Controller::IsWireless()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
	if(!joystick)
		return false;
	// a battery level means it runs on battery; wired and unknown don't rule out a cable
	switch(SDL_JoystickCurrentPowerLevel(joystick))
	{
		case SDL_JOYSTICK_POWER_EMPTY:
		case SDL_JOYSTICK_POWER_LOW:
		case SDL_JOYSTICK_POWER_MEDIUM:
		case SDL_JOYSTICK_POWER_FULL:
			return true;
		default:
			return false;
	}
#else
	return false;
#endif
}

void Controller::SetDualSenseRumble(uint8_t left, uint8_t right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	if(!is_dualsense && !is_dualsense_edge)
		return;
	DS5EffectsState_t state;
	SDL_zero(state);
	// Match SDL's DualSense driver: the Edge and firmware 0 (often all that is read over
	// Bluetooth) take the current rumble bits, which newer firmware needs to rumble at all
	if(!is_dualsense_edge && firmware_version != 0 && firmware_version < 0x0224)
	{
		state.ucEnableBits1 |= 0x01;
		state.ucRumbleLeft = left >> 1;
		state.ucRumbleRight = right >> 1;
	}
	else
	{
		state.ucEnableBits3 |= 0x04;
		state.ucRumbleLeft = left;
		state.ucRumbleRight = right;
	}
	state.rgucUnknown1[4] = manager->GetDualSenseIntensity();
	state.ucEnableBits2 |= 0x40;
	state.ucEnableBits1 |= 0x02;
	SDL_GameControllerSendEffect(controller, &state, sizeof(state));
#endif
}

void Controller::SetRumble(uint8_t left, uint8_t right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	if(is_dualsense || is_dualsense_edge)
		SetDualSenseRumble(left, right);
	else
		SDL_GameControllerRumble(controller, (uint16_t)left << 8, (uint16_t)right << 8, 5000);
#endif
}

void Controller::ChangeLEDColor(const uint8_t *led_color)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller || !has_led)
		return;
	SDL_GameControllerSetLED(controller, led_color[0], led_color[1], led_color[2]);
#endif
}

void Controller::ChangePlayerIndex(const uint8_t player_index)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	SDL_GameControllerSetPlayerIndex(controller, player_index);
#endif
}

void Controller::SetTriggerEffects(uint8_t type_left, const uint8_t *data_left, uint8_t type_right, const uint8_t *data_right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if((!is_dualsense && !is_dualsense_edge) || !controller)
		return;
	DS5EffectsState_t state;
	SDL_zero(state);
	state.rgucUnknown1[4] = manager->GetDualSenseIntensity();
	state.ucEnableBits2 |= 0x40;
	state.ucEnableBits1 |= (0x04 /* left trigger */ | 0x08 /* right trigger */);
	state.rgucLeftTriggerEffect[0] = type_left;
	SDL_memcpy(state.rgucLeftTriggerEffect + 1, data_left, 10);
	state.rgucRightTriggerEffect[0] = type_right;
	SDL_memcpy(state.rgucRightTriggerEffect + 1, data_right, 10);
	SDL_GameControllerSendEffect(controller, &state, sizeof(state));
#endif
}

void Controller::SetDualsenseMic(bool on)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if((!is_dualsense && !is_dualsense_edge) || !controller)
		return;
	DS5EffectsState_t state;
	SDL_zero(state);
	state.ucEnableBits2 |= 0x01 /* mic light */ | 0x02 /* mic */;
	if(on)
	{
		state.ucMicLightMode = 0x01;
		state.ucAudioMuteBits = 0x08;
	}
	else
	{
		state.ucMicLightMode = 0x00;
		state.ucAudioMuteBits = 0x00;
	}
	SDL_GameControllerSendEffect(controller, &state, sizeof(state));
#endif
}

void Controller::SetHapticRumble(uint16_t left, uint16_t right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	if(is_dualsense || is_dualsense_edge)
		SetDualSenseRumble(left >> 8, right >> 8);
	else
		SDL_GameControllerRumble(controller, left, right, 5000);
#endif
}

bool Controller::IsDualSense()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return is_dualsense;
#endif
	return false;
}

bool Controller::IsDualSenseEdge()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return is_dualsense_edge;
#endif
	return false;
}

bool Controller::IsHandheld()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return is_handheld;
#endif
	return false;
}

bool Controller::IsSteamVirtual()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return is_steam_virtual;
#endif
	return false;
}

bool Controller::IsSteamVirtualUnmasked()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return is_steam_virtual_unmasked;
#endif
	return false;
}

void Controller::resetMotionControls()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	chiaki_accel_new_zero_set_active(&accel_zero, real_accel.accel_x, real_accel.accel_y, real_accel.accel_z, false);
	chiaki_orientation_tracker_init(&orientation_tracker);
	chiaki_orientation_tracker_update(
		&orientation_tracker, state.gyro_x, state.gyro_y, state.gyro_z,
		real_accel.accel_x, real_accel.accel_y, real_accel.accel_z, &accel_zero, false, last_motion_timestamp);
	chiaki_orientation_tracker_apply_to_controller_state(&orientation_tracker, &state);
#endif
}
