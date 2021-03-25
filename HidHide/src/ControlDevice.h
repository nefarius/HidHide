// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// ControlDevice.h
#pragma once

EXTERN_C_START

// Create the control device and returns
_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideControlDeviceCreate(
    _In_ WDFDRIVER wdfDriver,
    _Out_ WDFDEVICE* wdfControlDevice);

// Notification handler called when a user-mode application calls DeviceIoControl or when another driver creates a request by calling either WdfIoTargetSendIoctlSynchronously or WdfIoTargetFormatRequestForIoctl
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL HidHideControlDeviceEvtIoDeviceControl;

EXTERN_C_END
