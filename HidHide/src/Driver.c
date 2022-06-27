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

_Use_decl_annotations_
NTSTATUS
HidHideDriverEvtDeviceAdd(WDFDRIVER wdfDriver, PWDFDEVICE_INIT wdfDeviceInit)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfDriver);

    NTSTATUS ntstatus;

    ntstatus = HidHideCreateDevice(wdfDeviceInit);
    return (ntstatus);
}
