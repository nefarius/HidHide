// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Driver.c
#include "stdafx.h"
#include "Driver.h"
#include "Device.h"
#include "Logging.h"
#include "Logic.h"

_Use_decl_annotations_
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
    WDF_DRIVER_CONFIG wdfDriverConfig;
    WDFDRIVER         wdfDriver;
    NTSTATUS          ntstatus;

    // Note use DBG_AND_RETURN_NTSTATUS for now till the ETW provides are registered
    // Note that INFO_LEVEL and TRACE_LEVEL are suppressed per default; only ERROR_LEVEL is immediately forwarded

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    WDF_DRIVER_CONFIG_INIT(&wdfDriverConfig, NULL);
    wdfDriverConfig.EvtDriverDeviceAdd = HidHideDriverEvtDeviceAdd;
    wdfDriverConfig.EvtDriverUnload    = HidHideDriverEvtUnload;
    ntstatus = WdfDriverCreate(pDriverObject, pRegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &wdfDriverConfig, &wdfDriver);
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("WdfDriverCreate", ntstatus);

    // Bail out when the environment is tampered with
    ntstatus = LogRegisterProviders();
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Allow logic to act
    ntstatus = OnDriverCreate(wdfDriver);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (ntstatus);
}

_Use_decl_annotations_
VOID HidHideDriverEvtUnload(WDFDRIVER wdfDriver)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfDriver);

    LogUnregisterProviders();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static BOOLEAN IsXboxSyntheticGamepadBus(_In_ PWDFDEVICE_INIT wdfDeviceInit)
{
    DECLARE_CONST_UNICODE_STRING(busId, L"SYNTHETICGAMEPAD");
    WDFMEMORY hardwareIds;
    size_t bufferSize;
    BOOLEAN matched = FALSE;

    NTSTATUS ntstatus = WdfFdoInitAllocAndQueryProperty(wdfDeviceInit, DevicePropertyHardwareID,
        NonPagedPoolNx, WDF_NO_OBJECT_ATTRIBUTES, &hardwareIds);
    if (!NT_SUCCESS(ntstatus)) return (FALSE);

    PCWSTR id = WdfMemoryGetBuffer(hardwareIds, &bufferSize);
    size_t remaining = bufferSize / sizeof(WCHAR);
    while (remaining != 0 && *id != L'\0')
    {
        size_t length;
        ntstatus = RtlStringCchLengthW(id, remaining, &length);
        if (!NT_SUCCESS(ntstatus)) break;

        UNICODE_STRING hardwareId;
        if (NT_SUCCESS(RtlUnicodeStringInit(&hardwareId, id)) && RtlEqualUnicodeString(&hardwareId, &busId, TRUE))
        {
            matched = TRUE;
            break;
        }

        id += length + 1;
        remaining -= length + 1;
    }

    WdfObjectDelete(hardwareIds);
    return (matched);
}

_Use_decl_annotations_
NTSTATUS
HidHideDriverEvtDeviceAdd(WDFDRIVER wdfDriver, PWDFDEVICE_INIT wdfDeviceInit)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfDriver);

    // The Xbox Synthetic Gamepad Bus is a UMDF/VHF source in HIDClass.
    // Leave its parent stack alone and continue filtering its controller children normally.
    if (IsXboxSyntheticGamepadBus(wdfDeviceInit))
    {
        TRACE_ALWAYS(L"Skipping the Xbox Synthetic Gamepad Bus");
        return (STATUS_SUCCESS);
    }

    return (HidHideCreateDevice(wdfDeviceInit));
}
