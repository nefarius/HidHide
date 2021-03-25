// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logging.h
#pragma once

// The define for ProjectDirLength is passed from the project file to the source code via a define
static_assert((sizeof(__FILE__) > ProjectDirLength), "Ensure any source code is located in a subdirectory of the project");

// Macros for tracing
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

// Log and trace a message
_IRQL_requires_same_
_IRQL_requires_max_(HIGH_LEVEL)
NTSTATUS LogEvent(_In_ NTSTRSAFE_PCSTR fileName, _In_ UINT32 lineNumber, _In_ NTSTRSAFE_PCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_z_ _Printf_format_string_ NTSTRSAFE_PCWSTR format, ...);

EXTERN_C_END
#else

// Class converting error codes into text and pin-pointing the location where an exception is thrown
class LogException
{
public:

    LogException() noexcept = delete;
    ~LogException() = default;
    LogException(_In_ LogException const& rhs) = delete;
    LogException(_In_ LogException && rhs) noexcept = delete;
    LogException& operator=(_In_ LogException const& rhs) = delete;
    LogException& operator=(_In_ LogException && rhs) = delete;

    // Maintain formatted string containing exception location, result type, and result
    inline explicit LogException(_In_ std::string&& encodedExceptionData) noexcept { std::swap(m_EncodedExceptionData, encodedExceptionData); }

    // Log the location from where the exception is being reported together with the exception text
    LogException const& Log(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event) noexcept;

    // Convert the error code where needed and get the result in the specified type
    CONFIGRET ToCONFIGRET() const noexcept;
    HRESULT   ToHRESULT()  const noexcept;
    NTSTATUS  ToNTSTATUS() const noexcept;
    DWORD     ToWIN32()    const noexcept;

    // Pin point the source code location where the exception is thrown together with the exception text
    static std::string EncodeCONFIGRET(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ CONFIGRET result) noexcept;
    static std::string EncodeHRESULT  (_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ HRESULT   result) noexcept;
    static std::string EncodeNTSTATUS (_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ NTSTATUS  result) noexcept;
    static std::string EncodeWIN32    (_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_ DWORD     result) noexcept;

    // Re-throw an exception and copy its message
    static std::string ExceptionMessage() noexcept;

private:

    std::string m_EncodedExceptionData;
};

// Macros for throwing exceptions
#define THROW_CONFIGRET(result) { throw std::runtime_error(LogException::EncodeCONFIGRET(ETW(Exception), result));            }
#define THROW_HRESULT(result)   { throw std::runtime_error(LogException::EncodeHRESULT  (ETW(Exception), result));            }
#define THROW_NTSTATUS(result)  { throw std::runtime_error(LogException::EncodeNTSTATUS (ETW(Exception), result));            }
#define THROW_WIN32(result)     { throw std::runtime_error(LogException::EncodeWIN32    (ETW(Exception), result));            }
#define THROW_WIN32_LAST_ERROR  { throw std::runtime_error(LogException::EncodeWIN32    (ETW(Exception), ::GetLastError()));  }

// Macros for logging from within an exception handler
#define LOGEXC_AND_CONTINUE              { LogException(LogException::ExceptionMessage()).Log(ETW(Exception));                         }
#define LOGEXC_AND_RETURN                { LogException(LogException::ExceptionMessage()).Log(ETW(Exception)); return;                 }
#define LOGEXC_AND_RETURN_RESULT(result) { LogException(LogException::ExceptionMessage()).Log(ETW(Exception)); return (result);        }
#define LOGEXC_AND_RETURN_CONFIGRET      { return (LogException(LogException::ExceptionMessage()).Log(ETW(Exception)).ToCONFIGRET());  }
#define LOGEXC_AND_RETURN_HRESULT        { return (LogException(LogException::ExceptionMessage()).Log(ETW(Exception)).ToHRESULT());    }
#define LOGEXC_AND_RETURN_NTSTATUS       { return (LogException(LogException::ExceptionMessage()).Log(ETW(Exception)).ToNTSTATUS());   }
#define LOGEXC_AND_RETURN_WIN32          { return (LogException(LogException::ExceptionMessage()).Log(ETW(Exception)).ToWIN32());      }

// Convert an NTSTATUS into a WIN32 error code
DWORD WIN32_FROM_NTSTATUS(_In_ NTSTATUS result) noexcept;

// Convert an NTSTATUS into a WIN32 error code
DWORD WIN32_FROM_CONFIGRET(_In_ CONFIGRET result) noexcept;

// DbgPrintEx equavalent for user mode applications
NTSTATUS DbgPrintEx(_In_ ULONG componentId, _In_ ULONG level, _In_z_ _Printf_format_string_ LPCSTR format, ...) noexcept;

// Register the ETW logging and tracing providers
NTSTATUS LogRegisterProviders() noexcept;

// Unregister the ETW logging and tracing providers
NTSTATUS LogUnregisterProviders() noexcept;

// Trace a message
NTSTATUS TraceEvent(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_opt_ LPCWSTR messageW, _In_opt_ LPCSTR messageA) noexcept;

// Log and trace a message
NTSTATUS LogEvent(_In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_z_ _Printf_format_string_ LPCWSTR format, ...) noexcept;

#endif
