#include "EtwTraceSession.hpp"

#include <initguid.h>

#include <evntrace.h>
#include <strsafe.h>

using nefarius::utilities::Win32Error;

namespace
{
    // HidHide.man — EtwProviderLogging
    DEFINE_GUID(GUID_HIDHIDE_DRIVER_LOGGING,
                0x6da3ffa4,
                0xab7d,
                0x40e2,
                0xb5,
                0x0c,
                0xc4,
                0xb4,
                0x1b,
                0xba,
                0x36,
                0xe3);

    // HidHide.man — EtwProviderTracing
    DEFINE_GUID(GUID_HIDHIDE_DRIVER_TRACING,
                0xd9f22586,
                0x7514,
                0x4164,
                0xbb,
                0x9b,
                0x5c,
                0x67,
                0xd5,
                0xbd,
                0x2b,
                0xc7);

    // HidHideClient.man — EtwProviderLogging
    DEFINE_GUID(GUID_HIDHIDE_CLIENT_LOGGING,
                0x1e545280,
                0x184f,
                0x4954,
                0xb1,
                0xe4,
                0x9b,
                0xf8,
                0x89,
                0x8f,
                0x25,
                0x71);

    // HidHideClient.man — EtwProviderTracing
    DEFINE_GUID(GUID_HIDHIDE_CLIENT_TRACING,
                0x3bea3f5d,
                0xf523,
                0x467e,
                0x91,
                0x2d,
                0xe0,
                0x1d,
                0xcf,
                0x6d,
                0xad,
                0xcf);

    // HidHideCLI.man — EtwProviderLogging
    DEFINE_GUID(GUID_HIDHIDE_CLI_LOGGING,
                0xe3df118f,
                0x1afc,
                0x4630,
                0x9f,
                0x03,
                0x6c,
                0x6f,
                0x73,
                0x84,
                0x87,
                0x38);

    // HidHideCLI.man — EtwProviderTracing
    DEFINE_GUID(GUID_HIDHIDE_CLI_TRACING,
                0x54c5b24f,
                0x321d,
                0x46b0,
                0xbd,
                0x2e,
                0x79,
                0xfd,
                0xf7,
                0xba,
                0xb7,
                0xac);

    // Manifest keyword masks OR'd (Always | Debugging | Performance | Detailed)
    constexpr ULONGLONG kTracingKeywordMaskAll = 0x1u | 0x2u | 0x4u | 0x8u;

    constexpr UCHAR kTraceLevel = TRACE_LEVEL_VERBOSE;
}

constexpr wchar_t EtwTraceSession::kSessionName[];

std::vector<GUID> EtwTraceSession::ProvidersForScope(const hidhide::diag::TraceScope scope)
{
    switch (scope)
    {
    case hidhide::diag::TraceScope::Full:
        return {
            GUID_HIDHIDE_DRIVER_LOGGING,
            GUID_HIDHIDE_DRIVER_TRACING,
            GUID_HIDHIDE_CLIENT_LOGGING,
            GUID_HIDHIDE_CLIENT_TRACING,
            GUID_HIDHIDE_CLI_LOGGING,
            GUID_HIDHIDE_CLI_TRACING,
        };
    case hidhide::diag::TraceScope::Driver:
        return {
            GUID_HIDHIDE_DRIVER_LOGGING,
            GUID_HIDHIDE_DRIVER_TRACING,
        };
    case hidhide::diag::TraceScope::Userspace:
        return {
            GUID_HIDHIDE_CLIENT_LOGGING,
            GUID_HIDHIDE_CLIENT_TRACING,
            GUID_HIDHIDE_CLI_LOGGING,
            GUID_HIDHIDE_CLI_TRACING,
        };
    }
    return {};
}

std::expected<void, Win32Error> EtwTraceSession::EnableProvider(const TRACEHANDLE session,
                                                                const GUID& providerGuid)
{
    const ULONGLONG anyKeyword = kTracingKeywordMaskAll;
    const ULONG status = EnableTraceEx2(
        session,
        &providerGuid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        kTraceLevel,
        anyKeyword,
        0,
        0,
        nullptr);

    if (status != ERROR_SUCCESS)
    {
        ::SetLastError(status);
        return std::unexpected(Win32Error("EnableTraceEx2"));
    }

    return {};
}

std::expected<void, Win32Error> EtwTraceSession::Start(const std::wstring& etlPath,
                                                       const hidhide::diag::TraceScope scope)
{
    if (_sessionHandle != 0)
    {
        ::SetLastError(ERROR_ALREADY_EXISTS);
        return std::unexpected(Win32Error("EtwTraceSession::Start"));
    }

    // Remove stale session from a previous crash / abrupt termination.
    {
        constexpr ULONG kCleanupBytes = sizeof(EVENT_TRACE_PROPERTIES) + 8192;
        std::vector<BYTE> cleanup(kCleanupBytes);
        auto* cp = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(cleanup.data());
        ZeroMemory(cp, kCleanupBytes);
        cp->Wnode.BufferSize = kCleanupBytes;
        cp->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        ControlTraceW(0, kSessionName, cp, EVENT_TRACE_CONTROL_STOP);
    }

    const std::wstring& sessionName = std::wstring(kSessionName);
    const ULONG nameChars = static_cast<ULONG>(sessionName.size() + 1);
    const ULONG pathChars = static_cast<ULONG>(etlPath.size() + 1);
    const ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + (nameChars + pathChars) * sizeof(WCHAR);

    std::vector<BYTE> buffer(bufferSize);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
    ZeroMemory(props, bufferSize);

    props->Wnode.BufferSize = bufferSize;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->Wnode.Guid = GUID{};
    props->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    props->MaximumFileSize = 128;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    props->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + nameChars * sizeof(WCHAR);

    const HRESULT hrName = StringCbCopyW(
        reinterpret_cast<wchar_t*>(buffer.data() + props->LoggerNameOffset),
        nameChars * sizeof(WCHAR),
        sessionName.c_str());
    if (!SUCCEEDED(hrName))
        return std::unexpected(Win32Error(static_cast<DWORD>(ERROR_INVALID_PARAMETER), std::string("StringCbCopyW logger")));

    const HRESULT hrPath = StringCbCopyW(
        reinterpret_cast<wchar_t*>(buffer.data() + props->LogFileNameOffset),
        pathChars * sizeof(WCHAR),
        etlPath.c_str());
    if (!SUCCEEDED(hrPath))
        return std::unexpected(Win32Error(static_cast<DWORD>(ERROR_INVALID_PARAMETER), std::string("StringCbCopyW log path")));

    const ULONG errStart = StartTraceW(&_sessionHandle, sessionName.c_str(), props);
    if (errStart != ERROR_SUCCESS)
    {
        _sessionHandle = 0;
        ::SetLastError(errStart);
        return std::unexpected(Win32Error("StartTraceW"));
    }

    const auto providers = ProvidersForScope(scope);
    for (const auto& g : providers)
    {
        if (const auto e = EnableProvider(_sessionHandle, g); !e)
        {
            [[maybe_unused]] const auto unusedStop = Stop();
            return e;
        }
    }

    return {};
}

std::expected<void, Win32Error> EtwTraceSession::Stop()
{
    if (_sessionHandle == 0)
        return {};

    const ULONG bufSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(WCHAR) * 512;
    std::vector<BYTE> buffer(bufSize);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
    ZeroMemory(props, bufSize);
    props->Wnode.BufferSize = bufSize;

    const TRACEHANDLE h = _sessionHandle;
    _sessionHandle = 0;

    const ULONG errStop = ControlTraceW(h, nullptr, props, EVENT_TRACE_CONTROL_STOP);

    if (errStop != ERROR_SUCCESS)
    {
        ::SetLastError(errStop);
        return std::unexpected(Win32Error("ControlTrace STOP"));
    }

    return {};
}
