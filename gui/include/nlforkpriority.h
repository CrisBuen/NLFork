// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef NLFORK_PRIORITY_H
#define NLFORK_PRIORITY_H

/**
 * NLFork "Modo máximo": run NLFork ahead of background apps (Discord, browsers, launchers).
 * On Windows this sets the process to high CPU priority, asks the graphics scheduler to run its
 * GPU work first, and keeps Windows from slowing it down to save power (EcoQoS), even when its
 * window is not in front. Off restores the normal values. Only this process is touched; nothing
 * is changed in Windows itself, and it all ends when NLFork closes. No-op on other systems.
 */
void NLForkApplyMaxPriority(bool enabled);

#endif // NLFORK_PRIORITY_H
