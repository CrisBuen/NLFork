// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_CONTROLLERMANAGER_H
#define CHIAKI_CONTROLLERMANAGER_H

#include <chiaki/controller.h>

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#include <chiaki/orientation.h>
#endif

#define PS_TOUCHPAD_MAXX 1920
#define PS_TOUCHPAD_MAXY 1079

class Controller;

// NLFork button remapping. A binding is one SDL game controller button or a combo of two,
// stored as a bit mask of SDL_GameControllerButton values. `ps` is a ChiakiControllerButton
// or CHIAKI_CONTROLLER_ANALOG_BUTTON_L2 / _R2 (pressed fully).
struct ButtonRemapEntry
{
	QString action;
	uint32_t ps;
	uint32_t mask;
};

class ControllerManager : public QObject
{
	Q_OBJECT

	friend class Controller;

	private:
		QVector<ButtonRemapEntry> button_remap;
		uint32_t remap_combo_bits = 0; // buttons that take part in some combo
		uint32_t remap_used_bits = 0;  // buttons that are bound to something
		bool button_capture = false;
		uint32_t capture_held = 0;
		uint32_t capture_mask = 0;
		void CaptureButton(uint32_t bit, bool down);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		QSet<SDL_JoystickID> available_controllers;
#endif
		QMap<int, Controller *> open_controllers;
		bool creating_controller_mapping;
		bool joystick_allow_background_events;
		bool is_app_active;
		int poll_hitch_count = 0;
		bool moved;
		uint8_t dualsense_intensity;

		void ControllerClosed(Controller *controller);
		void CheckMoved();

	private slots:
		void UpdateAvailableControllers();
		void HandleEvents();
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		void ControllerEvent(SDL_Event evt);
#endif

	public:
		static ControllerManager *GetInstance();
		// Times controller input was not read for >= 25 ms (GUI thread only)
		int PollHitchCount() const { return poll_hitch_count; }

		ControllerManager(QObject *parent = nullptr);
		~ControllerManager();
		void SetButtonsByPos();
		void SetAllowJoystickBackgroundEvents(bool enabled);
		void SetIsAppActive(bool active);
		void SetDualSenseIntensity(uint8_t intensity) { dualsense_intensity = intensity; };
		uint8_t GetDualSenseIntensity() { return dualsense_intensity; };
		void creatingControllerMapping(bool creating_controller_mapping);
		QSet<int> GetAvailableControllers();
		Controller *OpenController(int device_id);

		// Remapping. `spec` looks like "cross=a;touchpad=back+start;l2=paddle1"; actions that are
		// missing keep their default binding and an empty value leaves the action unbound.
		static QStringList RemapActions();
		static QString DefaultRemapBinding(const QString &action);
		static QString RemapBindingLabel(const QString &binding);
		static QString NormalizeRemapSpec(const QString &spec);
		void SetButtonRemap(const QString &spec);
		QString RemapBinding(const QString &action) const;
		// False when a connected controller lacks a button of the binding (e.g. "touchpad" on an Xbox pad)
		bool RemapBindingAvailable(const QString &binding) const;
		void StartButtonCapture();
		void CancelButtonCapture();
		bool IsCapturingButtons() const { return button_capture; }

	signals:
		void ButtonCaptured(const QString &binding);
		void AvailableControllersUpdated();
		void ControllerMoved();
		void AudioOutputDeviceAdded(const QString &name);
};

class Controller : public QObject
{
	Q_OBJECT

	friend class ControllerManager;

	private:
		Controller(int device_id, ControllerManager *manager);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		void UpdateState(SDL_Event event);
		void SetDualSenseRumble(uint8_t left, uint8_t right);
		bool HandleButtonEvent(SDL_ControllerButtonEvent event);
		bool HandleAxisEvent(SDL_ControllerAxisEvent event);
#if SDL_VERSION_ATLEAST(2, 0, 14)
		bool HandleSensorEvent(SDL_ControllerSensorEvent event);
		bool HandleTouchpadEvent(SDL_ControllerTouchpadEvent event);
#endif
#endif

		int ref;
		ControllerManager *manager;
		int id;
		ChiakiOrientationTracker orientation_tracker;
		ChiakiControllerState state;
		bool updating_mapping_button;
		bool enable_analog_stick_mapping;
		bool is_dualsense;
		bool is_handheld;
		bool is_steam_virtual;
		bool is_steam_virtual_unmasked;
		bool is_dualsense_edge;
		bool has_led;
		bool micbutton_push;
		uint16_t firmware_version;

		// Remapping state (SDL button bit masks)
		uint32_t held_sdl = 0;
		uint32_t grace_bits = 0;   // combo buttons waiting a moment before acting alone
		uint32_t combo_locked = 0; // buttons that already fired a combo, silent until released
		uint32_t tap_ps = 0;       // short press released during the grace time
		uint8_t analog_l2 = 0;
		uint8_t analog_r2 = 0;
		void RecomputeButtons();
		uint32_t SingleActionFor(uint32_t bit) const;
		void NotifyRemapChanged();

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		QMap<QPair<Sint64, Sint64>, uint8_t> touch_ids;
		SDL_GameController *controller;
		ChiakiAccelNewZero accel_zero;
		ChiakiAccelNewZero real_accel;
		uint32_t last_motion_timestamp;
#endif

	public:
		~Controller();

		void Ref();
		void Unref();

		bool IsConnected();
		int GetDeviceID();
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		SDL_GameController *GetController() { return controller; };
#endif
		QString GetName();
		QString GetVIDPIDString();
		QString GetType();
		bool IsPS();
		QString GetGUIDString();
		ChiakiControllerState GetState();
		void SetRumble(uint8_t left, uint8_t right);
		void SetTriggerEffects(uint8_t type_left, const uint8_t *data_left, uint8_t type_right, const uint8_t *data_right);
		void SetDualsenseMic(bool on);
		void SetHapticRumble(uint16_t left, uint16_t right);
		void StartUpdatingMapping();
		void IsUpdatingMappingButton(bool is_updating_mapping_button);
		void EnableAnalogStickMapping(bool enabled);
		void ChangeLEDColor(const uint8_t *led_color);
		void ChangePlayerIndex(const uint8_t player_index);
		bool IsDualSense();
		bool IsHandheld();
		bool IsSteamVirtual();
		bool IsSteamVirtualUnmasked();
		bool IsDualSenseEdge();
		bool IsWireless();
		void resetMotionControls();

	signals:
		void StateChanged();
		void MicButtonPush();
		void NewButtonMapping(QString button);
		void UpdatingControllerMapping(Controller* controller);
};
#endif // CHIAKI_CONTROLLERMANAGER_H
