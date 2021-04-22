// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Device.c
#include "stdafx.h"
#include "Device.h"
#include "ControlDevice.h"
#include "Driver.h"
#include "Logging.h"
#include "Logic.h"

_Use_decl_annotations_
NTSTATUS HidHideCreateDevice(PWDFDEVICE_INIT wdfDeviceInit)
{
    TRACE_ALWAYS(L"");

    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    WDF_FILEOBJECT_CONFIG wdfFileObjectConfig;
    WDFDEVICE             wdfDevice;
    WDF_IO_QUEUE_CONFIG   wdfIoQueueConfig;
    WDFQUEUE              wdfQueue;
    NTSTATUS              ntstatus;

    // This is a filter
    WdfFdoInitSetFilter(wdfDeviceInit); // PASSIVE_LEVEL

    WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);
    wdfObjectAttributes.SynchronizationScope = WdfSynchronizationScopeNone;
    WDF_FILEOBJECT_CONFIG_INIT(&wdfFileObjectConfig, OnDeviceFileCreate, WDF_NO_EVENT_CALLBACK, OnDeviceFileCleanup);
    WdfDeviceInitSetFileObjectConfig(wdfDeviceInit, &wdfFileObjectConfig, &wdfObjectAttributes);

    // Create the device
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfObjectAttributes, DEVICE_CONTEXT);
    wdfObjectAttributes.EvtCleanupCallback = OnDeviceContextCleanup;
    ntstatus = WdfDeviceCreate(&wdfDeviceInit, &wdfObjectAttributes, &wdfDevice);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDeviceCreate", ntstatus);

    // Register the device interface
    ntstatus = WdfDeviceCreateDeviceInterface(wdfDevice, &HidHideInterfaceGuid, NULL);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDeviceCreateDeviceInterface", ntstatus);

    // Create the I/O requests queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&wdfIoQueueConfig, WdfIoQueueDispatchParallel);
    wdfIoQueueConfig.EvtIoDefault = HidHideDeviceEvtIoDefault;
    ntstatus = WdfIoQueueCreate(wdfDevice, &wdfIoQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &wdfQueue);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfIoQueueCreate", ntstatus);

    // Initilize the device context for future usage
    ntstatus = OnDeviceCreate(wdfDevice);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID HidHideDeviceEvtIoDefault(WDFQUEUE wdfQueue, WDFREQUEST wdfRequest)
{
    TRACE_PERFORMANCE(L"");

    NTSTATUS ntstatus;

    ntstatus = OnDeviceIoDefault(WdfIoQueueGetDevice(wdfQueue), wdfQueue, wdfRequest);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRequestComplete(wdfRequest, ntstatus);
    }
}
