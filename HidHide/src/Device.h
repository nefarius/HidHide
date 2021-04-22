// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Device.h
#pragma once
#include "Logic.h"

EXTERN_C_START

_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

// Notification handler called with every I/O request from the queue, unless request - specific callback functions have also been registered
EVT_WDF_IO_QUEUE_IO_DEFAULT HidHideDeviceEvtIoDefault;

EXTERN_C_END
