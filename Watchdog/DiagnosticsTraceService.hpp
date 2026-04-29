#pragma once

#include "EtwTraceSession.hpp"
#include "HidHideDiagnosticsTypes.hpp"

#include <expected>
#include <mutex>
#include <string>

//
// Orchestrates Winevt channel tuning (README parity), single EtwTraceSession, and capture file paths.
//
class DiagnosticsTraceService
{
public:
    static DiagnosticsTraceService& Instance();

    std::expected<hidhide::diag::TraceStatusResponse, hidhide::diag::ApiError> GetStatus() const;

    std::expected<void, hidhide::diag::ApiError> Start(const hidhide::diag::TraceStartRequest& req);

    std::expected<hidhide::diag::TraceStopResponse, hidhide::diag::ApiError> Stop();

    [[nodiscard]] std::expected<std::wstring, hidhide::diag::ApiError> GetEtlPathForDownload() const;

    std::expected<void, hidhide::diag::ApiError> DiscardCapture();

private:
    DiagnosticsTraceService() = default;

    mutable std::mutex _mutex;
    hidhide::diag::TraceSessionState _state = hidhide::diag::TraceSessionState::Idle;
    std::optional<std::int64_t> _startedAtEpoch;
    std::optional<std::int64_t> _stoppedAtEpoch;
    std::wstring _etlPath;
    std::wstring _suggestedFileName;
    hidhide::diag::TraceScope _scope = hidhide::diag::TraceScope::Full;
    bool _tuneChannels = true;
    bool _didTuneChannels = false;
    EtwTraceSession _trace;

    static std::int64_t NowEpochSeconds();

    static std::wstring ResolveOutputDirectory();

    static std::wstring MakeUniqueEtlPath(const std::wstring& directory);
};
