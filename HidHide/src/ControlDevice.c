// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// ControlDevice.c
#include "stdafx.h"
#include "ControlDevice.h"
#include "Logging.h"
#include "Logic.h"

_Use_decl_annotations_
NTSTATUS HidHideControlDeviceCreate(WDFDRIVER wdfDriver, WDFDEVICE* wdfControlDevice)
{
    TRACE_ALWAYS(L"");

    PWDFDEVICE_INIT       wdfDeviceInit;
    WDF_FILEOBJECT_CONFIG wdfFileObjectConfig;
    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    WDF_IO_QUEUE_CONFIG   wdfIoQueueConfig;
    WDFQUEUE              wdfQueue;
    NTSTATUS              ntstatus;

    DECLARE_CONST_UNICODE_STRING(controlDeviceNtDeviceName,  CONTROL_DEVICE_NT_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(controlDeviceDosDeviceName, CONTROL_DEVICE_DOS_DEVICE_NAME);

    // Initialize the device init structure
    wdfDeviceInit = WdfControlDeviceInitAllocate(wdfDriver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX); // PASSIVE_LEVEL
    if (NULL == wdfDeviceInit) LOG_AND_RETURN_NTSTATUS(L"WdfControlDeviceInitAllocate", STATUS_INSUFFICIENT_RESOURCES);

    // Register the device name
    ntstatus = WdfDeviceInitAssignName(wdfDeviceInit, &controlDeviceNtDeviceName);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfDeviceInitFree(wdfDeviceInit);
        LOG_AND_RETURN_NTSTATUS(L"WdfDeviceInitAssignName", ntstatus);
    }

    // Register to shutdown notifications so we know when to destroy the control device enabling the driver to unload in turn
    WdfControlDeviceInitSetShutdownNotification(wdfDeviceInit, OnSystemShutdown, WdfDeviceShutdown);

    // Lock-out multiple clients interacting with this control device at the same time
    WdfDeviceInitSetExclusive(wdfDeviceInit, TRUE);

    // Create the device
    WDF_FILEOBJECT_CONFIG_INIT(&wdfFileObjectConfig, OnControlDeviceFileCreate, NULL, OnControlDeviceFileCleanup);
    WdfDeviceInitSetFileObjectConfig(wdfDeviceInit, &wdfFileObjectConfig, WDF_NO_OBJECT_ATTRIBUTES);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfObjectAttributes, CONTROL_DEVICE_CONTEXT);
    wdfObjectAttributes.EvtCleanupCallback = OnControlDeviceContextCleanup;
    ntstatus = WdfDeviceCreate(&wdfDeviceInit, &wdfObjectAttributes, wdfControlDevice);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfDeviceInitFree(wdfDeviceInit);
        LOG_AND_RETURN_NTSTATUS(L"WdfDeviceCreate", ntstatus);
    }

    // Register the DOS device name
    ntstatus = WdfDeviceCreateSymbolicLink(*wdfControlDevice, &controlDeviceDosDeviceName);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDeviceCreateSymbolicLink", ntstatus);

    // Create the I/O requests queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&wdfIoQueueConfig, WdfIoQueueDispatchParallel);
    wdfIoQueueConfig.EvtIoDeviceControl = HidHideControlDeviceEvtIoDeviceControl;
    ntstatus = WdfIoQueueCreate(*wdfControlDevice, &wdfIoQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &wdfQueue);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfIoQueueCreate", ntstatus);

    // Custom logic hook
    ntstatus = OnControlDeviceCreate(*wdfControlDevice);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Enable I/O control requests
    WdfControlFinishInitializing(*wdfControlDevice);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID HidHideControlDeviceEvtIoDeviceControl(WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");

    NTSTATUS ntstatus;

    ntstatus = OnControlDeviceIoDeviceControl(WdfIoQueueGetDevice(wdfQueue), wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRequestComplete(wdfRequest, ntstatus);
    }
}
