// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logic.c
#include "stdafx.h"
#include "Logic.h"
#include "Config.h"
#include "ControlDevice.h"
#include "Device.h"
#include "Logging.h"

// Unique memory pool tag for buffers
#define LOGIC_TAG '2gaT'

// The control device is created first and should be deleted last after having release the driver resources
// Releasing the control device is required else the driver will never unload
WDFDEVICE s_wdfControlDevice = NULL;

// The device is multi-threading hence we need a lock for the critical sections
// As a rule of thumb, don't use the control device context directly but instead use the methods below
WDFWAITLOCK s_criticalSectionLock = NULL;

_Use_decl_annotations_
NTSTATUS OnDriverCreate(WDFDRIVER wdfDriver)
{
    TRACE_ALWAYS(L"");

    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    NTSTATUS              ntstatus;

    // Do an integrity check on the btree algorithm
    ntstatus = HidHideVerifyInternalConsistency();
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Create the control device so that we have a shared context that is device independent
    ntstatus = HidHideControlDeviceCreate(wdfDriver, &s_wdfControlDevice);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Part of the code is multi-threaded so we need a lock for managing the critical section on the control device context
    WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);
    wdfObjectAttributes.ParentObject = s_wdfControlDevice;
    ntstatus = WdfWaitLockCreate(&wdfObjectAttributes, &s_criticalSectionLock);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfWaitLockCreate", ntstatus);

    // Subscribe to the create process and load image notifications
    ntstatus = PsSetCreateProcessNotifyRoutine(OnSystemProcessChange, FALSE);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"PsSetCreateProcessNotifyRoutine", ntstatus);
    ntstatus = PsSetLoadImageNotifyRoutine(OnSystemLoadImage);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"PsSetLoadImageNotifyRoutine", ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID OnSystemShutdown(WDFDEVICE wdfControlDevice)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);

    // Stop monitoring of load image notifications
    PsRemoveLoadImageNotifyRoutine(OnSystemLoadImage);
    PsSetCreateProcessNotifyRoutine(OnSystemProcessChange, TRUE);

    // Release the evaluation cache resources
    HidHideProcessIdsFlushWhitelistEvaluationCache(s_criticalSectionLock);

    // Enter shutdown state
    UpdateDataForControlDeviceDeletionAndDeleteControlDeviceWhenNeeded(0);
}

_Use_decl_annotations_
VOID OnControlDeviceContextCleanup(WDFOBJECT wdfControlDeviceObject)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDeviceObject);

    // No specific action needed for this device driver but we like the tracing for tracing purposes
}

_Use_decl_annotations_
VOID OnSystemProcessChange(HANDLE parentId, HANDLE processId, BOOLEAN create)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(parentId);

    // When a process is stopped we need to properly unregister it as we no longer need its information
    // Notice that we aren't receiving notifications for any process with enhanced security
    if (FALSE == create)
    {
        HidHideProcessIdUnregister(s_criticalSectionLock, processId);
    }
}

_Use_decl_annotations_
VOID OnSystemLoadImage(PUNICODE_STRING fullImageName, HANDLE processId, PIMAGE_INFO imageInfo)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(imageInfo);

    // Ignore load attempts by device drivers (process id zero) or when the image name isn't provided
    if ((NULL != fullImageName) && (NULL != processId))
    {
        // Only process the first load attempt as this is the executable; subsequent calls are for its load modules
        HidHideProcessIdRegister(s_criticalSectionLock, processId, fullImageName);
    }
}

_Use_decl_annotations_
NTSTATUS OnDeviceCreate(WDFDEVICE wdfDevice)
{
    TRACE_ALWAYS(L"");

    PDEVICE_CONTEXT pDeviceContext;
    NTSTATUS        ntstatus;

    // Get the device instance path of this device and cache it for future use
    pDeviceContext = DeviceGetContext(wdfDevice);
    ntstatus = HidHideDeviceInstancePath(wdfDevice, &pDeviceContext->deviceInstancePath); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    UpdateDataForControlDeviceDeletionAndDeleteControlDeviceWhenNeeded(1);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID OnDeviceContextCleanup(WDFOBJECT wdfDeviceObject)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfDeviceObject);

    PDEVICE_CONTEXT pDeviceContext;

    // Release context resources
    pDeviceContext = DeviceGetContext(wdfDeviceObject);
    WdfObjectDelete(pDeviceContext->deviceInstancePath);
    UpdateDataForControlDeviceDeletionAndDeleteControlDeviceWhenNeeded(-1);
}

_Use_decl_annotations_
VOID OnDeviceFileCreate(WDFDEVICE wdfDevice, WDFREQUEST wdfRequest, WDFFILEOBJECT wdfFileObject)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(wdfFileObject);

    WDF_REQUEST_SEND_OPTIONS wdfRequestSendOptions;
    PDEVICE_CONTEXT          pDeviceContext;
    UNICODE_STRING           deviceInstancePath;
    HANDLE                   processId;
    BOOLEAN                  accessDenied;
    BOOLEAN                  cacheHit;
    WDFMEMORY                wdfMemory;
    LPWSTR                   buffer;
    NTSTATUS                 ntstatus;

    // Get the device instance path
    pDeviceContext = DeviceGetContext(wdfDevice);
    WdfStringGetUnicodeString(pDeviceContext->deviceInstancePath, &deviceInstancePath);

    // Should access be granted to a particular client that attempts to access the device?
    // This is determined by;
    // - The process id of the caller       --> a system-level process is always granted access
    // - The active state of service        --> is hide-hide enabled or not?
    // - The device instance path of device --> is the device mentioned on the black-list?
    // - The full load image of the client  --> is the caller on the white-list?

    processId    = PsGetCurrentProcessId();
    accessDenied = FALSE;
    if ((SYSTEM_PID != PROCESS_HANDLE_TO_PROCESS_ID(processId)) && (GetActive()) && (Blacklisted(&deviceInstancePath)))
    {
        // When the service is active, and the process is not a system-process, and the device being accessed is on the black-list, then the final verdict comes from the white-list
        if (Whitelisted(processId, &cacheHit))
        {
            // Log the first-time that a white-listed application is granted access to a black-listed device
            if (!cacheHit) LogEvent(ETW(Whitelisted), L"%ld", PROCESS_HANDLE_TO_PROCESS_ID(processId));
        }
        else
        {
            // Trace the first-time that an application is denied access to a black-listed device
            if (!cacheHit) TRACE_ALWAYS(L"Device is black-listed hence deny access");
            accessDenied = TRUE;
        }
        // Trace the first-time the device instance path is challenged, in support of the message above
        if (!cacheHit)
        {
            ntstatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPool, LOGIC_TAG, sizeof(WCHAR) * ((size_t)deviceInstancePath.Length + 1), &wdfMemory, &buffer);
            if (NT_SUCCESS(ntstatus)) ntstatus = RtlStringCchCopyUnicodeStringEx(buffer, ((size_t)deviceInstancePath.Length + 1), &deviceInstancePath, NULL, NULL, STRSAFE_NO_TRUNCATION);
            if (NT_SUCCESS(ntstatus)) TRACE_ALWAYS(buffer);
            WdfObjectDelete(wdfMemory);
        }
    }

    // Handle the request accordingly
    // Note that a file create has to be handled synchrounously
    if (accessDenied)
    {
        WdfRequestComplete(wdfRequest, STATUS_ACCESS_DENIED);
    }
    else
    {
        // Attach a completion routine and forward the original request to the driver lower in the driver stack
        // Note that WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET can't be used here as a bug check triggers when running driver verifier 
        WdfRequestFormatRequestUsingCurrentType(wdfRequest);
        WdfRequestSetCompletionRoutine(wdfRequest, OnDeviceFileCreateRequestCompletion, WDF_NO_CONTEXT);
        WDF_REQUEST_SEND_OPTIONS_INIT(&wdfRequestSendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&wdfRequestSendOptions, WDF_REL_TIMEOUT_IN_SEC(60));
        ntstatus = WdfRequestAllocateTimer(wdfRequest);
        if (!NT_SUCCESS(ntstatus))
        {
            // Bail out when we don't have the resources to forward the request
            WdfRequestComplete(wdfRequest, ntstatus);
        }
        else
        {
            // Bail out when we failed forwarding the request
            if (FALSE == WdfRequestSend(wdfRequest, WdfDeviceGetIoTarget(wdfDevice), &wdfRequestSendOptions))
            {
                WdfRequestComplete(wdfRequest, WdfRequestGetStatus(wdfRequest));
            }
        }
    }
}

_Use_decl_annotations_
VOID OnDeviceFileCreateRequestCompletion(WDFREQUEST wdfRequest, WDFIOTARGET wdfIoTarget, PWDF_REQUEST_COMPLETION_PARAMS wdfRequestCompletionParams, WDFCONTEXT wdfContext)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(wdfIoTarget);
    UNREFERENCED_PARAMETER(wdfContext);

    WdfRequestComplete(wdfRequest, wdfRequestCompletionParams->IoStatus.Status);
}

_Use_decl_annotations_
VOID OnDeviceFileCleanup(WDFFILEOBJECT wdfFileObject)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(wdfFileObject);

    // No specific action needed for this device driver but we like the tracing for tracing purposes
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceCreate(WDFDEVICE wdfControlDevice)
{
    TRACE_ALWAYS(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    NTSTATUS                ntstatus;

    pControlDeviceContext = ControlDeviceGetContext(wdfControlDevice);
    pControlDeviceContext->numberOfDevicesCreated = 0;
    pControlDeviceContext->shutdownPending = FALSE;

    // Query the multi-string property containing the white-listed full image names
    DECLARE_CONST_UNICODE_STRING(whitelistedFullImageNames, DRIVER_PROPERTY_WHITELISTED_FULL_IMAGE_NAMES);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&whitelistedFullImageNames, &pControlDeviceContext->whitelistedFullImageNames);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Query the multi-string property containing the black-listed device instance paths
    DECLARE_CONST_UNICODE_STRING(blacklistedDeviceInstancePaths, DRIVER_PROPERTY_BLACKLISTED_DEVICE_INSTANCE_PATHS);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&blacklistedDeviceInstancePaths, &pControlDeviceContext->blacklistedDeviceInstancePaths);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Query the boolean property indicating the activity state
    DECLARE_CONST_UNICODE_STRING(active, DRIVER_PROPERTY_ACTIVE);
    ntstatus = HidHideDriverGetBooleanProperty(&active, &pControlDeviceContext->active);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Log the activity state
    if (pControlDeviceContext->active)  LogEvent(ETW(Enabled), L"");
    if (!pControlDeviceContext->active) LogEvent(ETW(Disabled), L"");

    // Query the boolean property indicating the inverse state
    DECLARE_CONST_UNICODE_STRING(whitelistedInverse, DRIVER_PROPERTY_WHITELISTED_INVERSE);
    ntstatus = HidHideDriverGetBooleanProperty(&whitelistedInverse, &pControlDeviceContext->whitelistedInverse);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID OnControlDeviceFileCreate(WDFDEVICE wdfControlDevice, WDFREQUEST wdfRequest, WDFFILEOBJECT wdfFileObject)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfFileObject);

    // No specific action needed for this device driver
    // Note that we have to complete the I/O request!
    WdfRequestComplete(wdfRequest, STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID OnControlDeviceFileCleanup(WDFFILEOBJECT wdfFileObject)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfFileObject);

    // No specific action needed for this device driver but we like the tracing for tracing purposes
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoDeviceControl(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_PERFORMANCE(L"");

    switch (ioControlCode)
    {
    case IOCTL_GET_WHITELIST:
        return (OnControlDeviceIoGetWhitelist(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_SET_WHITELIST:
        return (OnControlDeviceIoSetWhitelist(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_GET_BLACKLIST:
        return (OnControlDeviceIoGetBlacklist(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_SET_BLACKLIST:
        return (OnControlDeviceIoSetBlacklist(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_GET_ACTIVE:
        return (OnControlDeviceIoGetActive(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_SET_ACTIVE:
        return (OnControlDeviceIoSetActive(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_GET_WLINVERSE:
        return (OnControlDeviceIoGetInverse(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    case IOCTL_SET_WLINVERSE:
        return (OnControlDeviceIoSetInverse(wdfControlDevice, wdfQueue, wdfRequest, outputBufferLength, inputBufferLength, ioControlCode));
        break;
    default:
        LOG_AND_RETURN_NTSTATUS(L"OnControlDeviceIoDeviceControl", STATUS_INVALID_PARAMETER);
    }
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoGetWhitelist(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    LPWSTR   buffer;
    size_t   neededSizeInCharacters;
    NTSTATUS ntstatus;

    // Validate buffer and, on success, report the current status
    if (0 != inputBufferLength) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    if (0 == outputBufferLength)
    {
        // No output buffer, then only report on the size needed
        ntstatus = GetWhitelist(NULL, 0, &neededSizeInCharacters);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }
    else
    {
        // With an output buffer provided, try to fill it
        ntstatus = WdfRequestRetrieveOutputBuffer(wdfRequest, outputBufferLength, &buffer, NULL);
        if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
        ntstatus = GetWhitelist(buffer, (outputBufferLength / sizeof(WCHAR)), &neededSizeInCharacters);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, (neededSizeInCharacters * sizeof(WCHAR)));
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoSetWhitelist(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(inputBufferLength);
    UNREFERENCED_PARAMETER(ioControlCode);

    LPWSTR   buffer;
    NTSTATUS ntstatus;

    // Validate buffer and
    if (0 != outputBufferLength) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    if (0 == inputBufferLength)
    {
        // No input then clear the list
        ntstatus = SetWhitelist(L"\0\0", 2);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }
    else
    {
        // Get the list provided
        ntstatus = WdfRequestRetrieveInputBuffer(wdfRequest, inputBufferLength, &buffer, NULL);
        if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveInputBuffer", ntstatus);
        ntstatus = SetWhitelist(buffer, (inputBufferLength / sizeof(WCHAR)));
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, inputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoGetBlacklist(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    LPWSTR   buffer;
    size_t   neededSizeInCharacters;
    NTSTATUS ntstatus;

    // Validate buffer and, on success, report the current status
    if (0 != inputBufferLength) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    if (0 == outputBufferLength)
    {
        // No output buffer, then only report on the size needed
        ntstatus = GetBlacklist(NULL, 0, &neededSizeInCharacters);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }
    else
    {
        // With an output buffer provided, try to fill it
        ntstatus = WdfRequestRetrieveOutputBuffer(wdfRequest, outputBufferLength, &buffer, NULL);
        if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
        ntstatus = GetBlacklist(buffer, (outputBufferLength / sizeof(WCHAR)), &neededSizeInCharacters);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, (neededSizeInCharacters * sizeof(WCHAR)));
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoSetBlacklist(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(inputBufferLength);
    UNREFERENCED_PARAMETER(ioControlCode);

    LPWSTR   buffer;
    NTSTATUS ntstatus;

    // Validate buffer and
    if (0 != outputBufferLength) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    if (0 == inputBufferLength)
    {
        // No input then clear the list
        ntstatus = SetBlacklist(L"\0\0", 2);
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }
    else
    {
        // Get the list provided
        ntstatus = WdfRequestRetrieveInputBuffer(wdfRequest, inputBufferLength, &buffer, NULL);
        if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveInputBuffer", ntstatus);
        ntstatus = SetBlacklist(buffer, (inputBufferLength / sizeof(WCHAR)));
        if (!NT_SUCCESS(ntstatus)) return (ntstatus);
    }

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, inputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoGetActive(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    PBOOLEAN buffer;
    NTSTATUS ntstatus;

    // Validate buffer and, on success, report the current status
    if ((0 != inputBufferLength) || (sizeof(BOOLEAN) != outputBufferLength)) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    ntstatus = WdfRequestRetrieveOutputBuffer(wdfRequest, outputBufferLength, &buffer, NULL);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
    *buffer = GetActive();

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, outputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoSetActive(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    PBOOLEAN buffer;
    NTSTATUS ntstatus;

    // Validate buffer, retrieve its content, and set the state accordingly
    if ((sizeof(BOOLEAN) != inputBufferLength) || (0 != outputBufferLength)) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    ntstatus = WdfRequestRetrieveInputBuffer(wdfRequest, inputBufferLength, &buffer, NULL);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
    ntstatus = SetActive(FALSE != *buffer);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, outputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoGetInverse(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    PBOOLEAN buffer;
    NTSTATUS ntstatus;

    // Validate buffer and, on success, report the current status
    if ((0 != inputBufferLength) || (sizeof(BOOLEAN) != outputBufferLength)) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    ntstatus = WdfRequestRetrieveOutputBuffer(wdfRequest, outputBufferLength, &buffer, NULL);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
    *buffer = GetInverse();

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, outputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS OnControlDeviceIoSetInverse(WDFDEVICE wdfControlDevice, WDFQUEUE wdfQueue, WDFREQUEST wdfRequest, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wdfControlDevice);
    UNREFERENCED_PARAMETER(wdfQueue);
    UNREFERENCED_PARAMETER(ioControlCode);

    PBOOLEAN buffer;
    NTSTATUS ntstatus;

    // Validate buffer, retrieve its content, and set the state accordingly
    if ((sizeof(BOOLEAN) != inputBufferLength) || (0 != outputBufferLength)) LOG_AND_RETURN_NTSTATUS(L"Validation", STATUS_INVALID_PARAMETER);
    ntstatus = WdfRequestRetrieveInputBuffer(wdfRequest, inputBufferLength, &buffer, NULL);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfRequestRetrieveOutputBuffer", ntstatus);
    ntstatus = SetInverse(FALSE != *buffer);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    WdfRequestCompleteWithInformation(wdfRequest, STATUS_SUCCESS, outputBufferLength);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
BOOLEAN Whitelisted(HANDLE processId, BOOLEAN* cacheHit)
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;

    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    BOOLEAN result = (STATUS_PROCESS_IN_JOB == HidHideProcessIdCheckFullImageNameAgainstWhitelist(s_criticalSectionLock, processId, pControlDeviceContext->whitelistedFullImageNames, cacheHit));
    return GetInverse() ? !result : result;
}

_Use_decl_annotations_
BOOLEAN Blacklisted(PUNICODE_STRING deviceInstancePath)
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    UNICODE_STRING          blacklistedDeviceInstancePath;

    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    for (ULONG index1 = 0, size1 = WdfCollectionGetCount(pControlDeviceContext->blacklistedDeviceInstancePaths); (index1 < size1); index1++)
    {
        WdfStringGetUnicodeString(WdfCollectionGetItem(pControlDeviceContext->blacklistedDeviceInstancePaths, index1), &blacklistedDeviceInstancePath); // PASSIVE_LEVEL
        if (0 == RtlCompareUnicodeString(&blacklistedDeviceInstancePath, deviceInstancePath, TRUE))
        {
            WdfWaitLockRelease(s_criticalSectionLock);
            return (TRUE);
        }
    }
    WdfWaitLockRelease(s_criticalSectionLock);

    return (FALSE);
}

_Use_decl_annotations_
NTSTATUS GetWhitelist(LPWSTR buffer, size_t bufferSizeInCharacters, size_t* neededSizeInCharacters)
{
    TRACE_ALWAYS(L"");

    WDFCOLLECTION wdfCollection;
    NTSTATUS      ntstatus;

    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_WHITELISTED_FULL_IMAGE_NAMES);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&parameterName, &wdfCollection);
    if (NT_SUCCESS(ntstatus))
    {
        ntstatus = HidHideCollectionToMultiString(wdfCollection, buffer, bufferSizeInCharacters, neededSizeInCharacters);
        WdfObjectDelete(wdfCollection);
    }
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS SetWhitelist(LPWSTR buffer, size_t bufferSizeInCharacters)
{
    TRACE_ALWAYS(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    NTSTATUS                ntstatus;

    // Persist the new setting in the registry
    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_WHITELISTED_FULL_IMAGE_NAMES);
    ntstatus = HidHideDriverSetMultiStringProperty(&parameterName, buffer, bufferSizeInCharacters);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Dispose the old setting and apply the new setting
    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    WdfObjectDelete(pControlDeviceContext->whitelistedFullImageNames);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&parameterName, &pControlDeviceContext->whitelistedFullImageNames);
    WdfWaitLockRelease(s_criticalSectionLock);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Flush the evaluation cache as it is no longer accurate
    HidHideProcessIdsFlushWhitelistEvaluationCache(s_criticalSectionLock);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS GetBlacklist(LPWSTR buffer, size_t bufferSizeInCharacters, size_t* neededSizeInCharacters)
{
    TRACE_ALWAYS(L"");

    WDFCOLLECTION wdfCollection;
    NTSTATUS      ntstatus;

    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_BLACKLISTED_DEVICE_INSTANCE_PATHS);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&parameterName, &wdfCollection);
    if (NT_SUCCESS(ntstatus))
    {
        ntstatus = HidHideCollectionToMultiString(wdfCollection, buffer, bufferSizeInCharacters, neededSizeInCharacters);
        WdfObjectDelete(wdfCollection);
    }
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS SetBlacklist(LPWSTR buffer, size_t bufferSizeInCharacters)
{
    TRACE_ALWAYS(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    NTSTATUS                ntstatus;

    // Persist the new setting in the registry
    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_BLACKLISTED_DEVICE_INSTANCE_PATHS);
    ntstatus = HidHideDriverSetMultiStringProperty(&parameterName, buffer, bufferSizeInCharacters);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Dispose the old setting and apply the new setting
    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    WdfObjectDelete(pControlDeviceContext->blacklistedDeviceInstancePaths);
    ntstatus = HidHideDriverCreateCollectionForMultiStringProperty(&parameterName, &pControlDeviceContext->blacklistedDeviceInstancePaths);
    WdfWaitLockRelease(s_criticalSectionLock);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
BOOLEAN GetActive()
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    BOOLEAN                 active;

    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    active = pControlDeviceContext->active;
    WdfWaitLockRelease(s_criticalSectionLock);

    return (active);
}

_Use_decl_annotations_
NTSTATUS SetActive(BOOLEAN active)
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    BOOLEAN                 changed;
    NTSTATUS                ntstatus;

    // Persist the new setting in the registry
    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_ACTIVE);
    ntstatus = HidHideDriverSetBooleanProperty(&parameterName, active);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Apply the new setting
    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    changed = (pControlDeviceContext->active != active);
    pControlDeviceContext->active = active;
    WdfWaitLockRelease(s_criticalSectionLock);

    // Log service active changes
    if ((changed) && (active))  LogEvent(ETW(Enabled),  L"");
    if ((changed) && (!active)) LogEvent(ETW(Disabled), L"");

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
BOOLEAN GetInverse()
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    BOOLEAN                 inverse;

    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    inverse = pControlDeviceContext->whitelistedInverse;
    WdfWaitLockRelease(s_criticalSectionLock);

    return (inverse);
}

_Use_decl_annotations_
NTSTATUS SetInverse(BOOLEAN inverse)
{
    TRACE_PERFORMANCE(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    BOOLEAN                 changed;
    NTSTATUS                ntstatus;

    // Persist the new setting in the registry
    DECLARE_CONST_UNICODE_STRING(parameterName, DRIVER_PROPERTY_WHITELISTED_INVERSE);
    ntstatus = HidHideDriverSetBooleanProperty(&parameterName, inverse);
    if (!NT_SUCCESS(ntstatus)) return (ntstatus);

    // Apply the new setting
    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);
    changed = (pControlDeviceContext->whitelistedInverse != inverse);
    pControlDeviceContext->whitelistedInverse = inverse;
    WdfWaitLockRelease(s_criticalSectionLock);

    // Log service inverse changes
    if ((changed) && (inverse))  LogEvent(ETW(Enabled), L"");
    if ((changed) && (!inverse)) LogEvent(ETW(Disabled), L"");

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID UpdateDataForControlDeviceDeletionAndDeleteControlDeviceWhenNeeded(INT32 increment)
{
    TRACE_ALWAYS(L"");

    PCONTROL_DEVICE_CONTEXT pControlDeviceContext;
    BOOLEAN                 deleteControlDevice;

    WdfWaitLockAcquire(s_criticalSectionLock, NULL);
    pControlDeviceContext = ControlDeviceGetContext(s_wdfControlDevice);

    // Memorize the fact that we are in a shutdown state
    if (0 == increment) pControlDeviceContext->shutdownPending = TRUE;

    // When we are in a shutdown state and this is the last device then we should delete the control device
    pControlDeviceContext->numberOfDevicesCreated += increment;
    deleteControlDevice = (((0 == pControlDeviceContext->numberOfDevicesCreated) && (pControlDeviceContext->shutdownPending)) ? TRUE : FALSE);
    WdfWaitLockRelease(s_criticalSectionLock);

    // Delete the control device either when
    // - we just entered the shutdown state and have no devices pending or
    // - we are in the shutdown state and the last device is deleted
    if (deleteControlDevice)
    {
        // For an unknown reason we never seem to reach this particular point in the code when testing the implementation in VM-ware
        // The cleanup of the control device is a pre-requirisite for releasing the driver itself. As the code below isn't reached, the
        // control device context cleanup isn't done (hence the reason for moving most of the cleanup code to the system shutdown event)
        // and with the control device alive the driver unload is probably also never called.
        // Deleting the control device directly from within the system shutdown notification does force a cleanup but results in a bug
        // check when Verifier is enabled. This is valid as per Microsoft documentation, the control device context should be maintained
        // till the last filter device is gone. Apparently the operating system unloads the driver itself before we reach this point.
        TRACE_ALWAYS(L"");
        WdfObjectDelete(s_wdfControlDevice);
        s_wdfControlDevice = NULL;
    }
}
