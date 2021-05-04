// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logging.cpp
#include "stdafx.h"
#include "Logging.h"

namespace
{
    constexpr auto PrefixCONFIGRET { "CONFIGRET"  };
    constexpr auto PrefixHRESULT   { "HRESULT"    };
    constexpr auto PrefixNTSTATUS  { "NTSTATUS"   };
    constexpr auto PrefixWIN32     { "Error code" };

    enum class Encoding
    {
        unknown,
        configret,
        hresult,
        ntstatus,
        win32
    };

    struct DecodedExceptionData
    {
        Encoding encoding;
        union
        {
            CONFIGRET configret;
            HRESULT   hresult;
            NTSTATUS  ntstatus;
            DWORD     win32;
        };
    };

    // Decode the encoded exception data
    DecodedExceptionData DecodeExceptionData(_In_ std::string const& encodedExceptionData) noexcept
    {
        try
        {
            std::istringstream is{ encodedExceptionData };
            is.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
            std::string prefix;
            DecodedExceptionData decodedExceptionData{};
            is >> prefix >> std::hex >> decodedExceptionData.hresult;
            decodedExceptionData.encoding = ((PrefixWIN32 == prefix) ? Encoding::win32 : (PrefixHRESULT == prefix) ? Encoding::hresult : (PrefixNTSTATUS == prefix) ? Encoding::ntstatus : (PrefixCONFIGRET == prefix) ? Encoding::configret : Encoding::unknown);
            return (decodedExceptionData);
        }
        catch (...)
        {
            DecodedExceptionData const decodedExceptionData{};
            return (decodedExceptionData);
        }
    }

    // cfgmgr32 provides no FormetMessage support for retrieving a message message for a given result code
    // As such, we have to provide the table ourselfs bused on the information provided in cfgmgr32.h
    constexpr LPCSTR DescriptionTable[] =
    {
        "success",                              // CR_SUCCESS                                        (0x00000000)
        "default",                              // CR_DEFAULT                                        (0x00000001)
        "out of memory",                        // CR_OUT_OF_MEMORY                                  (0x00000002)
        "invalid pointer",                      // CR_INVALID_POINTER                                (0x00000003)
        "invalid flag",                         // CR_INVALID_FLAG                                   (0x00000004)
        "invalid device node",                  // CR_INVALID_DEVNODE + CR_INVALID_DEVINST           (0x00000005)
        "invalid resource description",         // CR_INVALID_RES_DES                                (0x00000006)
        "invalid logical configuration",        // CR_INVALID_LOG_CONF                               (0x00000007)
        "invalid arbitrator",                   // CR_INVALID_ARBITRATOR                             (0x00000008)
        "invalid node list",                    // CR_INVALID_NODELIST                               (0x00000009)
        "device node has requests",             // CR_DEVNODE_HAS_REQS + CR_DEVINST_HAS_REQS         (0x0000000A)
        "invalid resource id",                  // CR_INVALID_RESOURCEID                             (0x0000000B)
        "dlvxd not found",                      // CR_DLVXD_NOT_FOUND                                (0x0000000C)
        "no such device node",                  // CR_NO_SUCH_DEVNODE + CR_NO_SUCH_DEVINST           (0x0000000D)
        "no more logical configurations",       // CR_NO_MORE_LOG_CONF                               (0x0000000E)
        "no more resource descriptions",        // CR_NO_MORE_RES_DES                                (0x0000000F)
        "already such device node",             // CR_ALREADY_SUCH_DEVNODE + CR_ALREADY_SUCH_DEVINST (0x00000010)
        "invalid range list",                   // CR_INVALID_RANGE_LIST                             (0x00000011)
        "invalid range",                        // CR_INVALID_RANGE                                  (0x00000012)
        "failure",                              // CR_FAILURE                                        (0x00000013)
        "no such logical device",               // CR_NO_SUCH_LOGICAL_DEV                            (0x00000014)
        "create blocked",                       // CR_CREATE_BLOCKED                                 (0x00000015)
        "not system VM",                        // CR_NOT_SYSTEM_VM                                  (0x00000016)
        "remove vetoed",                        // CR_REMOVE_VETOED                                  (0x00000017)
        "APM vetoed",                           // CR_APM_VETOED                                     (0x00000018)
        "invalid load type",                    // CR_INVALID_LOAD_TYPE                              (0x00000019)
        "buffer small",                         // CR_BUFFER_SMALL                                   (0x0000001A)
        "no arbitrator",                        // CR_NO_ARBITRATOR                                  (0x0000001B)
        "no registry handle",                   // CR_NO_REGISTRY_HANDLE                             (0x0000001C)
        "registry error",                       // CR_REGISTRY_ERROR                                 (0x0000001D)
        "invalid device id",                    // CR_INVALID_DEVICE_ID                              (0x0000001E)
        "invalid data",                         // CR_INVALID_DATA                                   (0x0000001F)
        "invalid api",                          // CR_INVALID_API                                    (0x00000020)
        "device loader not ready",              // CR_DEVLOADER_NOT_READY                            (0x00000021)
        "need restart",                         // CR_NEED_RESTART                                   (0x00000022)
        "no more hardware profiles",            // CR_NO_MORE_HW_PROFILES                            (0x00000023)
        "device not there",                     // CR_DEVICE_NOT_THERE                               (0x00000024)
        "no such value",                        // CR_NO_SUCH_VALUE                                  (0x00000025)
        "wrong type",                           // CR_WRONG_TYPE                                     (0x00000026)
        "invalid priority",                     // CR_INVALID_PRIORITY                               (0x00000027)
        "not disableble",                       // CR_NOT_DISABLEABLE                                (0x00000028)
        "free resources",                       // CR_FREE_RESOURCES                                 (0x00000029)
        "query vetoed",                         // CR_QUERY_VETOED                                   (0x0000002A)
        "can't share IRQ",                      // CR_CANT_SHARE_IRQ                                 (0x0000002B)
        "no dependent",                         // CR_NO_DEPENDENT                                   (0x0000002C)
        "same resources",                       // CR_SAME_RESOURCES                                 (0x0000002D)
        "no such registry key",                 // CR_NO_SUCH_REGISTRY_KEY                           (0x0000002E)
        "invalid machine name",                 // CR_INVALID_MACHINENAME                            (0x0000002F)
        "remote communication failure",         // CR_REMOTE_COMM_FAILURE                            (0x00000030)
        "machine unavailable",                  // CR_MACHINE_UNAVAILABLE                            (0x00000031)
        "no configuration management services", // CR_NO_CM_SERVICES                                 (0x00000032)
        "access denied",                        // CR_ACCESS_DENIED                                  (0x00000033)
        "call not implemented",                 // CR_CALL_NOT_IMPLEMENTED                           (0x00000034)
        "invalid property",                     // CR_INVALID_PROPERTY                               (0x00000035)
        "device interface active",              // CR_DEVICE_INTERFACE_ACTIVE                        (0x00000036)
        "no such device interface",             // CR_NO_SUCH_DEVICE_INTERFACE                       (0x00000037)
        "invalid reference string",             // CR_INVALID_REFERENCE_STRING                       (0x00000038)
        "invalid conflict list",                // CR_INVALID_CONFLICT_LIST                          (0x00000039)
        "invalid index",                        // CR_INVALID_INDEX                                  (0x0000003A)
        "invalid structure size"                // CR_INVALID_STRUCTURE_SIZE                         (0x0000003B)
    };

    // Add a logging and tracing entry
    void UpdateEventLog(_In_ bool traceOnly, _In_ LPCSTR fileName, _In_ UINT32 lineNumber, _In_opt_ LPCSTR functionName, _In_ PCEVENT_DESCRIPTOR event, _In_opt_ LPCWSTR messageW, _In_opt_ LPCSTR messageA)
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (nullptr == functionName) functionName = "";
        if (nullptr == messageW) messageW = L"";
        if (nullptr == messageA) messageA = "";

        std::vector<EVENT_DATA_DESCRIPTOR> eventDataDescriptor(5);
        ::EventDataDescCreate(&eventDataDescriptor.at(0), fileName,     static_cast<ULONG>(sizeof(CHAR) * (::strlen(fileName) + 1)));
        ::EventDataDescCreate(&eventDataDescriptor.at(1), &lineNumber,  static_cast<ULONG>(sizeof(lineNumber)));
        ::EventDataDescCreate(&eventDataDescriptor.at(2), functionName, static_cast<ULONG>(sizeof(CHAR) * (::strlen(functionName) + 1)));
        ::EventDataDescCreate(&eventDataDescriptor.at(3), messageW,     static_cast<ULONG>(sizeof(WCHAR) * (::wcslen(messageW) + 1)));
        ::EventDataDescCreate(&eventDataDescriptor.at(4), messageA,     static_cast<ULONG>(sizeof(CHAR) * (::strlen(messageA) + 1)));

        if (traceOnly)
        {
            // Trace the entry
            if (auto const result{ ::EventWriteTransfer(EtwProviderTracing_Context.RegistrationHandle, event, nullptr, nullptr, static_cast<ULONG>(eventDataDescriptor.size()), eventDataDescriptor.data()) }; (ERROR_SUCCESS != result)) THROW_WIN32(result);
        }
        else
        {
            // Log the entry
            if (auto const result{ ::EventWriteTransfer(EtwProviderLogging_Context.RegistrationHandle, event, nullptr, nullptr, static_cast<ULONG>(eventDataDescriptor.size()), eventDataDescriptor.data()) }; (ERROR_SUCCESS != result)) THROW_WIN32(result);

            // Trace the entry after having copied the relevant information from the log entry
            EVENT_DESCRIPTOR eventDescriptor;
            eventDescriptor.Id      = event->Id;
            eventDescriptor.Version = EtwEventTraceAlways.Version;
            eventDescriptor.Channel = EtwEventTraceAlways.Channel;
            eventDescriptor.Opcode  = EtwEventTraceAlways.Opcode;
            eventDescriptor.Level   = event->Level;
            eventDescriptor.Opcode  = EtwEventTraceAlways.Opcode;
            eventDescriptor.Task    = EtwTaskLog;
            eventDescriptor.Keyword = EtwEventTraceAlways.Keyword;

            // Trace the entry
            if (auto const result{ ::EventWriteTransfer(EtwProviderTracing_Context.RegistrationHandle, &eventDescriptor, nullptr, nullptr, static_cast<ULONG>(eventDataDescriptor.size()), eventDataDescriptor.data()) }; (ERROR_SUCCESS != result)) THROW_WIN32(result);
        }
    }
}

_Use_decl_annotations_
LogException const& LogException::Log(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        UpdateEventLog(false, fileName, lineNumber, functionName, event, L"", m_EncodedExceptionData.c_str());
        return (*this);
    }
    catch (...)
    {
        // Avoid a recursive loop and don't log here
        return (*this);
    }
}

CONFIGRET LogException::ToCONFIGRET() const noexcept
{
    try
    {
        auto const decodedExceptionData{ DecodedExceptionData() };
        switch (decodedExceptionData.encoding)
        {
        case Encoding::unknown:   break;
        case Encoding::configret: return (decodedExceptionData.configret);
        case Encoding::hresult:   return ((SUCCEEDED(decodedExceptionData.hresult)) ? CR_SUCCESS : CR_FAILURE);
        case Encoding::ntstatus:  return ((NT_SUCCESS(decodedExceptionData.ntstatus)) ? CR_SUCCESS : CR_FAILURE);
        case Encoding::win32:     return ((SUCCEEDED(HRESULT_FROM_WIN32(decodedExceptionData.win32))) ? CR_SUCCESS : CR_FAILURE);
        }
        return (CR_FAILURE);
    }
    catch (...)
    {
        return (CR_FAILURE);
    }
}

HRESULT LogException::ToHRESULT() const noexcept
{
    try
    {
        auto const decodedExceptionData{ DecodedExceptionData() };
        switch (decodedExceptionData.encoding)
        {
        case Encoding::unknown:   break;
        case Encoding::configret: return (HRESULT_FROM_WIN32(WIN32_FROM_CONFIGRET(decodedExceptionData.configret)));
        case Encoding::hresult:   return (decodedExceptionData.hresult);
        case Encoding::ntstatus:  return (HRESULT_FROM_NT(decodedExceptionData.ntstatus));
        case Encoding::win32:     return (HRESULT_FROM_WIN32(decodedExceptionData.win32));
        }
        return (HRESULT_FROM_NT(STATUS_UNSUCCESSFUL));
    }
    catch (...)
    {
        return (HRESULT_FROM_NT(STATUS_UNHANDLED_EXCEPTION));
    }
}

NTSTATUS LogException::ToNTSTATUS() const noexcept
{
    try
    {
        auto const decodedExceptionData{ DecodedExceptionData() };
        switch (decodedExceptionData.encoding)
        {
        case Encoding::unknown:   break;
        case Encoding::configret: return (HRESULT_FROM_WIN32(WIN32_FROM_CONFIGRET(decodedExceptionData.configret)));
        case Encoding::hresult:   return (decodedExceptionData.hresult);
        case Encoding::ntstatus:  return (decodedExceptionData.ntstatus);
        case Encoding::win32:     return (HRESULT_FROM_WIN32(decodedExceptionData.win32));
        }
        return (STATUS_UNSUCCESSFUL);
    }
    catch (...)
    {
        return (STATUS_UNHANDLED_EXCEPTION);
    }
}

DWORD LogException::ToWIN32() const noexcept
{
    try
    {
        auto const decodedExceptionData{ DecodedExceptionData() };
        switch (decodedExceptionData.encoding)
        {
        case Encoding::unknown:   break;
        case Encoding::configret: return (WIN32_FROM_CONFIGRET(decodedExceptionData.configret));
        case Encoding::hresult:   return (WIN32_FROM_NTSTATUS(decodedExceptionData.hresult));
        case Encoding::ntstatus:  return (WIN32_FROM_NTSTATUS(decodedExceptionData.ntstatus));
        case Encoding::win32:     return (decodedExceptionData.win32);
        }
        return (WIN32_FROM_NTSTATUS(STATUS_UNSUCCESSFUL));
    }
    catch (...)
    {
        return (ERROR_UNHANDLED_EXCEPTION);
    }
}

_Use_decl_annotations_
std::string LogException::EncodeCONFIGRET(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, CONFIGRET result) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (nullptr == functionName) functionName = "";

        std::ostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << PrefixCONFIGRET << " 0x" << std::hex << std::setfill('0') << std::setw(8) << std::uppercase << result << " at " << fileName << "(" << std::dec << lineNumber << ") " << functionName << ": " << ((result < _countof(DescriptionTable)) ? DescriptionTable[result] : "unknown result code");
        return (os.str());
    }
    catch (...)
    {
        return (std::string{});
    }
}

_Use_decl_annotations_
std::string LogException::EncodeHRESULT(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, HRESULT result) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (nullptr == functionName) functionName = "";

        std::vector<CHAR> buffer(LOGGING_MESSAGE_MAXIMUM_SIZE);
        ::FormatMessageA((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
        std::ostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << PrefixHRESULT << " 0x" << std::hex << std::setfill('0') << std::setw(8) << std::uppercase << result << " at " << fileName << "(" << std::dec << lineNumber << ") " << functionName << ": " << buffer.data();
        return (os.str());
    }
    catch (...)
    {
        return (std::string{});
    }
}

_Use_decl_annotations_
std::string LogException::EncodeNTSTATUS(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, NTSTATUS result) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (nullptr == functionName) functionName = "";

        std::unique_ptr<std::remove_pointer<HMODULE>::type, decltype(&::FreeLibrary)> hmodule(::LoadLibraryA("ntdll.dll"), &::FreeLibrary);
        std::vector<CHAR> buffer(LOGGING_MESSAGE_MAXIMUM_SIZE);
        ::FormatMessageA((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE), hmodule.get(), static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
        std::ostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << PrefixNTSTATUS << " 0x" << std::hex << std::setfill('0') << std::setw(4) << std::uppercase << result << " at " << fileName << "(" << std::dec << lineNumber << ") " << functionName << ": " << buffer.data();
        return (os.str());
    }
    catch (...)
    {
        return (std::string{});
    }
}

_Use_decl_annotations_
std::string LogException::EncodeWIN32(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, DWORD result) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (nullptr == functionName) functionName = "";

        std::vector<CHAR> buffer(LOGGING_MESSAGE_MAXIMUM_SIZE);
        ::FormatMessageA((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
        std::ostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << PrefixWIN32 << " 0x" << std::hex << std::setfill('0') << std::setw(4) << std::uppercase << result << " at " << fileName << "(" << std::dec << lineNumber << ") " << functionName << ": " << buffer.data();
        return (os.str());
    }
    catch (...)
    {
        return (std::string{});
    }
}

std::string LogException::ExceptionMessage() noexcept
{
    try
    {
        try
        {
            throw;
        }
        catch (std::exception const& exc)
        {
            return (std::string(exc.what()));
        }
    }
    catch (...)
    {
        return (std::string{});
    }
}

_Use_decl_annotations_
DWORD WIN32_FROM_NTSTATUS(NTSTATUS result) noexcept
{
    try
    {
        typedef ULONG(WINAPI RtlNtStatusToDosError)(NTSTATUS status);
        std::unique_ptr<std::remove_pointer<HMODULE>::type, decltype(&::FreeLibrary)> hmodule(::LoadLibraryA("ntdll.dll"), &::FreeLibrary);
#pragma warning(suppress: 4191 26490) // We can't avoid the use of a reinterpret cast here as GetProcAddress is by itself unsafe
        auto const function(reinterpret_cast<RtlNtStatusToDosError*>(::GetProcAddress(hmodule.get(), "RtlNtStatusToDosError")));
        if (nullptr == function) THROW_WIN32_LAST_ERROR;
        return (static_cast<DWORD>(function(result)));
    }
    catch (...)
    {
        return (ERROR_UNHANDLED_EXCEPTION);
    }
}

_Use_decl_annotations_
DWORD WIN32_FROM_CONFIGRET(CONFIGRET result) noexcept
{
    try
    {
        return (::CM_MapCrToWin32Err(result, WIN32_FROM_NTSTATUS(STATUS_UNSUCCESSFUL)));
    }
    catch (...)
    {
        return (WIN32_FROM_NTSTATUS(STATUS_UNSUCCESSFUL));
    }
}

_Use_decl_annotations_
NTSTATUS DbgPrintEx(ULONG componentId, ULONG level, LPCSTR format, ...) noexcept
{
    try
    {
        UNREFERENCED_PARAMETER(componentId);
        UNREFERENCED_PARAMETER(level);
        if (nullptr == format) THROW_WIN32(ERROR_INVALID_PARAMETER);

        va_list args;
        va_start(args, format);
        std::vector<CHAR> buffer(LOGGING_MESSAGE_MAXIMUM_SIZE);
        std::vsnprintf(buffer.data(), buffer.size(), format, args);
        ::OutputDebugStringA(buffer.data());
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        return (STATUS_UNHANDLED_EXCEPTION);
    }
}

NTSTATUS LogRegisterProviders() noexcept
{
    try
    {
        EventRegisterNefarius_Hid_Hide_CLI();
        EventRegisterNefarius_Drivers_HidHideCLI();

        // The define for BldProductVersion is passed from the project file to the source code via a define
        ::LogEvent(ETW(Started), L"%s", _L(BldProductVersion));
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("LogRegisterProviders", STATUS_UNHANDLED_EXCEPTION);
    }
}

NTSTATUS LogUnregisterProviders() noexcept
{
    try
    {
        ::LogEvent(ETW(Stopped), L"");
        EventUnregisterNefarius_Drivers_HidHideCLI();
        EventUnregisterNefarius_Hid_Hide_CLI();
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("LogUnregisterProviders", STATUS_UNHANDLED_EXCEPTION);
    }
}

_Use_decl_annotations_
NTSTATUS TraceEvent(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, LPCWSTR messageW, LPCSTR messageA) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        UpdateEventLog(true, fileName, lineNumber, functionName, event, messageW, messageA);
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("TraceEvent", STATUS_UNHANDLED_EXCEPTION);
    }
}

_Use_decl_annotations_
NTSTATUS LogEvent(LPCSTR fileName, UINT32 lineNumber, LPCSTR functionName, PCEVENT_DESCRIPTOR event, LPCWSTR format, ...) noexcept
{
    try
    {
        if ((nullptr == fileName) || (nullptr == event) || (nullptr == format)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        va_list args;
        va_start(args, format);
        std::vector<WCHAR> messageW(LOGGING_MESSAGE_MAXIMUM_SIZE);
        std::vswprintf(messageW.data(), messageW.size(), format, args);
        UpdateEventLog(false, fileName, lineNumber, functionName, event, messageW.data(), "");
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("LogEvent", STATUS_UNHANDLED_EXCEPTION);
    }
}
