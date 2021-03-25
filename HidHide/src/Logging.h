// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logging.h
#pragma once

// Macros for tracing
// The define for ProjectDirLength is passed from the project file to the source code via a define
#define TRACE_DETAILED(message)    { TraceEvent(&__FILE__[ProjectDirLength], __LINE__, __FUNCTION__, &EtwEventTraceDetailed,    message, ""); if (MCGEN_EVENT_ENABLED(EtwEventTraceDetailed)    && MCGEN_EVENT_ENABLED(EtwEventTraceDebugging)) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s(%d) %s\n", &__FILE__[ProjectDirLength], __LINE__, __FUNCTION__); }
#define TRACE_PERFORMANCE(message) { TraceEvent(&__FILE__[ProjectDirLength], __LINE__, __FUNCTION__, &EtwEventTracePerformance, message, ""); if (MCGEN_EVENT_ENABLED(EtwEventTracePerformance) && MCGEN_EVENT_ENABLED(EtwEventTraceDebugging)) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s(%d) %s\n", &__FILE__[ProjectDirLength], __LINE__, __FUNCTION__); }
#define TRACE_ALWAYS(message)      { TraceEvent(&__FILE__[ProjectDirLength], __LINE__, __FUNCTION__, &EtwEventTraceAlways,      message, ""); if (MCGEN_EVENT_ENABLED(EtwEventTraceAlways)      && MCGEN_EVENT_ENABLED(EtwEventTraceDebugging)) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s(%d) %s\n", &__FILE__[ProjectDirLength], __LINE__, __FUNCTION__); }

// Macro for a shorter notation on the LogEvent parameter list
#define ETW(eventDescriptor) &__FILE__[ProjectDirLength], __LINE__, __FUNCTION__, &EtwEventLog##eventDescriptor

// Macros for logging
#define LOG_AND_RETURN_NTSTATUS(message, result) { LogEvent(ETW(Exception), L"%s reports NT status 0x%08X", message, result); return (result); }

// Macros for debugging messages (for logging internally or when logging isn't yet available)
#define DBG_AND_RETURN_NTSTATUS(message, result) { DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s reports NT status 0x%08X\n", message, result); return (result); } // DIRQL

// The maximum logging message length supported (as TRACE_MESSAGE_MAXIMUM_SIZE is too large)
#define LOGGING_MESSAGE_MAXIMUM_SIZE 200

#if MCGEN_USE_KERNEL_MODE_APIS
EXTERN_C_START

// Register the ETW logging and tracing providers
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS LogRegisterProviders();

// Unregister the ETW logging and tracing providers
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS LogUnregisterProviders();

// Trace a message
_IRQL_requires_same_
_IRQL_requires_max_(HIGH_LEVEL)
NTSTATUS TraceEvent(_In_ NTSTRSAFE_PCSTR fileName, _In_ UINT32 lineNumber, _In_ NTSTRSAFE_PCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ NTSTRSAFE_PCWSTR messageW, _In_ NTSTRSAFE_PCSTR messageA);

// Log an entry and trace a message
_IRQL_requires_same_
_IRQL_requires_max_(HIGH_LEVEL)
NTSTATUS LogEvent(_In_ NTSTRSAFE_PCSTR fileName, _In_ UINT32 lineNumber, _In_ NTSTRSAFE_PCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_z_ _Printf_format_string_ NTSTRSAFE_PCWSTR format, ...);

EXTERN_C_END
#else

// DbgPrintEx equavalent for user mode applications
NTSTATUS DbgPrintEx(_In_ ULONG componentId, _In_ ULONG level, _In_z_ _Printf_format_string_ LPCSTR format, ...) noexcept;

// Register the ETW logging and tracing providers
NTSTATUS LogRegisterProviders() noexcept;

// Unregister the ETW logging and tracing providers
NTSTATUS LogUnregisterProviders() noexcept;

// Trace a message
NTSTATUS TraceEvent(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ LPCWSTR messageW, _In_ LPCSTR messageA) noexcept;

// Format a message and add it to the event log and trace file
NTSTATUS LogEvent(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_z_ _Printf_format_string_ LPCWSTR format, ...) noexcept;

#endif
