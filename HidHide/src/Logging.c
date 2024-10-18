// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logging.c
#include "stdafx.h"
#include "Logging.h"

// Write to log/trace file
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS LogWriteTransfer(_In_ PMCGEN_TRACE_CONTEXT context, _In_ PCEVENT_DESCRIPTOR eventDescriptor, _In_ PCSTR fileName, _In_ UINT32 lineNumber, _In_ PCSTR functionName, _In_ PCWSTR messageW, _In_ PCSTR messageA)
{
    EVENT_DATA_DESCRIPTOR eventDataDescriptor[6];
    USHORT UNALIGNED const* traits;

    EventDataDescCreate(&eventDataDescriptor[1], fileName,     (ULONG)(strlen(fileName) + 1));
    EventDataDescCreate(&eventDataDescriptor[2], &lineNumber,  (ULONG)(sizeof(unsigned int)));
    EventDataDescCreate(&eventDataDescriptor[3], functionName, (ULONG)(strlen(functionName) + 1));
    EventDataDescCreate(&eventDataDescriptor[4], messageW,     (ULONG)(wcslen(messageW) + 1) * sizeof(WCHAR));
    EventDataDescCreate(&eventDataDescriptor[5], messageA,     (ULONG)(strlen(messageA) + 1));

    // The part below is taken from the generated message compiler code (McGenEventWrite)
    traits = (USHORT UNALIGNED*)(UINT_PTR)EtwProviderTracing_Context.Logger;
    if (NULL == traits)
    {
        eventDataDescriptor[0].Ptr      = 0;
        eventDataDescriptor[0].Size     = 0;
        eventDataDescriptor[0].Reserved = 0;
    }
    else
    {
        eventDataDescriptor[0].Ptr      = (ULONG_PTR)traits;
        eventDataDescriptor[0].Size     = *traits;
        eventDataDescriptor[0].Reserved = 2; // EVENT_DATA_DESCRIPTOR_TYPE_PROVIDER_METADATA
    }

    return (MCGEN_EVENTWRITETRANSFER(context->RegistrationHandle, eventDescriptor, NULL, NULL, _countof(eventDataDescriptor), &eventDataDescriptor[0]));
}

_Use_decl_annotations_
NTSTATUS LogRegisterProviders()
{
    NTSTATUS ntstatus;

    ntstatus = EventRegisterNefarius_HidHide(); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("EventRegister logging", ntstatus);
    ntstatus = EventRegisterNefarius_Drivers_HidHide();
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("EventRegister tracing", ntstatus);

    // The define for BldProductVersion is passed from the project file to the source code via a define
    LogEvent(ETW(Started), L"%s", _L(BldProductVersion));

    return (ntstatus);
}

_Use_decl_annotations_
NTSTATUS LogUnregisterProviders()
{
    NTSTATUS ntstatus;

    LogEvent(ETW(Stopped), L"");

    ntstatus = EventUnregisterNefarius_Drivers_HidHide(); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("EventRegister tracing", ntstatus);
    ntstatus = EventUnregisterNefarius_HidHide();
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("EventRegister logging", ntstatus);

    return (ntstatus);
}

_Use_decl_annotations_
NTSTATUS TraceEvent(NTSTRSAFE_PCSTR fileName, UINT32 lineNumber, NTSTRSAFE_PCSTR functionName, PCEVENT_DESCRIPTOR event, NTSTRSAFE_PCWSTR messageW, NTSTRSAFE_PCSTR messageA)
{
    return (LogWriteTransfer(&EtwProviderTracing_Context, event, fileName, lineNumber, functionName, messageW, messageA));
}

_Use_decl_annotations_
NTSTATUS LogEvent(NTSTRSAFE_PCSTR fileName, const UINT32 lineNumber, NTSTRSAFE_PCSTR functionName, PCEVENT_DESCRIPTOR event, NTSTRSAFE_PCWSTR format, ...)
{
    EVENT_DESCRIPTOR eventDescriptor;
    WCHAR            buffer[LOGGING_MESSAGE_MAXIMUM_SIZE];
    va_list          args;
    NTSTATUS         ntstatus;

    // Create message
    va_start(args, format);
    ntstatus = RtlStringCchVPrintfW(&buffer[0], _countof(buffer), format, args);
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("RtlStringCchVPrintfW", ntstatus);

    // Log the entry
    ntstatus = LogWriteTransfer(&EtwProviderLogging_Context, event, fileName, lineNumber, functionName, &buffer[0], ""); // HIGH_LEVEL
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("LogWriteTransfer logging", ntstatus);

    // Trace the entry after having copied the relevant information from the log entry
    eventDescriptor.Id      = event->Id;
    eventDescriptor.Version = EtwEventTraceAlways.Version;
    eventDescriptor.Channel = EtwEventTraceAlways.Channel;
    eventDescriptor.Opcode  = EtwEventTraceAlways.Opcode;
    eventDescriptor.Level   = event->Level;
    eventDescriptor.Opcode  = EtwEventTraceAlways.Opcode;
    eventDescriptor.Task    = EtwTaskLog;
    eventDescriptor.Keyword = EtwEventTraceAlways.Keyword;

    // Trace the entry
    ntstatus = LogWriteTransfer(&EtwProviderTracing_Context, &eventDescriptor, fileName, lineNumber, functionName, &buffer[0], "");
    if (!NT_SUCCESS(ntstatus)) DBG_AND_RETURN_NTSTATUS("LogWriteTransfer tracing", ntstatus);

    return (ntstatus);
}
