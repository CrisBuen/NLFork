// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "nlforkpriority.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui);

#ifdef _WIN32
#include <windows.h>

// D3DKMT_SCHEDULINGPRIORITYCLASS values (d3dkmthk.h is not part of the regular Windows headers)
enum
{
	NLFORK_GPU_PRIORITY_NORMAL = 2,
	NLFORK_GPU_PRIORITY_HIGH = 4,
};
typedef LONG (APIENTRY *SetGpuPriorityFn)(HANDLE process, int priority);

void NLForkApplyMaxPriority(bool enabled)
{
	HANDLE process = GetCurrentProcess();

	// CPU: HIGH, not REALTIME, so input, audio and Windows itself keep running normally
	const bool cpu_ok = SetPriorityClass(process, enabled ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS) != 0;
	const DWORD cpu_error = cpu_ok ? 0 : GetLastError();

	// Power: with ControlMask set and StateMask 0, Windows never runs NLFork in its power saving
	// mode (EcoQoS), even when another window is in front. ControlMask 0 hands it back to Windows.
	PROCESS_POWER_THROTTLING_STATE throttling = {};
	throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	throttling.ControlMask = enabled ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;
	throttling.StateMask = 0;
	const bool power_ok = SetProcessInformation(process, ProcessPowerThrottling, &throttling, sizeof(throttling)) != 0;
	const DWORD power_error = power_ok ? 0 : GetLastError();

	// GPU: the graphics scheduler runs NLFork's decode and render work before other apps' work
	bool gpu_ok = false;
	LONG gpu_status = 0;
	HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
	if(!gdi)
		gdi = LoadLibraryW(L"gdi32.dll");
	if(gdi)
	{
		auto set_gpu_priority = reinterpret_cast<SetGpuPriorityFn>(
			reinterpret_cast<void *>(GetProcAddress(gdi, "D3DKMTSetProcessSchedulingPriorityClass")));
		if(set_gpu_priority)
		{
			gpu_status = set_gpu_priority(process, enabled ? NLFORK_GPU_PRIORITY_HIGH : NLFORK_GPU_PRIORITY_NORMAL);
			gpu_ok = gpu_status >= 0;
		}
	}

	QString msg = QStringLiteral("[nlfork] Max mode %1: cpu priority %2").arg(enabled ? "on" : "off", enabled ? "high" : "normal");
	if(!cpu_ok)
		msg += QStringLiteral(" FAILED error=%1").arg(cpu_error);
	msg += QStringLiteral(", gpu priority %1").arg(enabled ? "high" : "normal");
	if(!gpu_ok)
		msg += QStringLiteral(" FAILED status=0x%1").arg(static_cast<quint32>(gpu_status), 0, 16);
	msg += QStringLiteral(", power throttling %1").arg(enabled ? "off" : "managed by Windows");
	if(!power_ok)
		msg += QStringLiteral(" FAILED error=%1").arg(power_error);
	qCInfo(chiakiGui).noquote() << msg;
}

#else

void NLForkApplyMaxPriority(bool) {}

#endif
