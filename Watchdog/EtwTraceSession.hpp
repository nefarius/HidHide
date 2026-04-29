#pragma once

#include "NefLibIncludes.hpp"
#include <evntrace.h>

#include "HidHideDiagnosticsTypes.hpp"

//
// Single-instance user-mode file trace session for HidHide ETW providers only.
//
class EtwTraceSession
{
public:
    static constexpr wchar_t kSessionName[] = L"HidHideFieldTrace";

    std::expected<void, nefarius::utilities::Win32Error> Start(
        const std::wstring& etlPath,
        hidhide::diag::TraceScope scope);

    std::expected<void, nefarius::utilities::Win32Error> Stop();

    [[nodiscard]] bool IsActive() const noexcept { return _sessionHandle != 0; }

private:
    TRACEHANDLE _sessionHandle = 0;

    static std::vector<GUID> ProvidersForScope(hidhide::diag::TraceScope scope);

    static std::expected<void, nefarius::utilities::Win32Error> EnableProvider(TRACEHANDLE session,
                                                                                 const GUID& providerGuid);
};
