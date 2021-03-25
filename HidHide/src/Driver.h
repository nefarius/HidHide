// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Driver.h
#pragma once

EXTERN_C_START

// Main entry point of device driver called after a driver is loaded and responsible for initializing the driver
DRIVER_INITIALIZE DriverEntry;

// Notification handler called when the driver has no more device objects after it handles an IRP_MN_REMOVE_DEVICE request
EVT_WDF_DRIVER_UNLOAD HidHideDriverEvtUnload;

// Notification handler called after a bus driver detects a device that has a hardware identifier (ID) matching a hardware ID this driver supports
EVT_WDF_DRIVER_DEVICE_ADD HidHideDriverEvtDeviceAdd;

EXTERN_C_END
