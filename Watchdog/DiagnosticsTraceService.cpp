#include "DiagnosticsTraceService.hpp"

#include "ETWUtil.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
    std::string WideToUtf8(const std::wstring_view w)
    {
        if (w.empty())
            return {};

        const int nbytes = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            w.data(),
            static_cast<int>(w.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (nbytes <= 0)
            return {};

        std::string out(static_cast<size_t>(nbytes), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            w.data(),
            static_cast<int>(w.size()),
            out.data(),
            nbytes,
            nullptr,
            nullptr);

        return out;
    }

    constexpr wchar_t kChannelDriver[] = L"Nefarius-Drivers-HidHide/Diagnostic";
    constexpr wchar_t kChannelClient[] = L"Nefarius-Drivers-HidHideClient/Diagnostic";
    constexpr wchar_t kChannelCli[] = L"Nefarius-Drivers-HidHideCLI/Diagnostic";

    hidhide::diag::ApiError MakeApiError(std::string code, std::string message, const unsigned long win32 = 0)
    {
        hidhide::diag::ApiError e;
        e.code = std::move(code);
        e.message = std::move(message);
        if (win32 != 0)
            e.win32 = win32;
        return e;
    }

    void InvokeTune(const wchar_t* channel, const bool restore)
    {
        const auto r = restore
                           ? etw::utils::RestoreReadmeDefaultDiagnosticVisibility(channel)
                           : etw::utils::ApplyReadmeMaximumDiagnosticVisibility(channel);
        if (!r)
        {
            spdlog::warn("Diagnostics channel {} failed: {}",
                         restore ? "restore" : "tune",
                         r.error().getErrorMessageA());
        }
    }

    void TuneScopeChannels(const hidhide::diag::TraceScope scope, const bool restore)
    {
        switch (scope)
        {
        case hidhide::diag::TraceScope::Full:
            InvokeTune(kChannelDriver, restore);
            InvokeTune(kChannelClient, restore);
            InvokeTune(kChannelCli, restore);
            break;
        case hidhide::diag::TraceScope::Driver:
            InvokeTune(kChannelDriver, restore);
            break;
        case hidhide::diag::TraceScope::Userspace:
            InvokeTune(kChannelClient, restore);
            InvokeTune(kChannelCli, restore);
            break;
        }
    }
}

DiagnosticsTraceService& DiagnosticsTraceService::Instance()
{
    static DiagnosticsTraceService s;
    return s;
}

std::int64_t DiagnosticsTraceService::NowEpochSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::wstring DiagnosticsTraceService::ResolveOutputDirectory()
{
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"PROGRAMDATA", buf, MAX_PATH);
    std::wstring root;
    if (n > 0 && n < MAX_PATH)
        root.assign(buf, n);
    else
        root = L"C:\\ProgramData";

    const fs::path dir = fs::path(root) / L"Nefarius" / L"HidHide" / L"ETW";
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
    {
        spdlog::error("Failed to create ETW output directory: {}", ec.message());
    }
    return dir.wstring();
}

std::wstring DiagnosticsTraceService::MakeUniqueEtlPath(const std::wstring& directory)
{
    SYSTEMTIME st{};
    GetSystemTime(&st);
    const unsigned tick = GetTickCount();
    wchar_t name[160] = {};
    swprintf_s(
        name,
        L"HidHide-trace-%04u%02u%02u-%02u%02u%02u-%08x.etl",
        static_cast<unsigned>(st.wYear),
        static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        tick);

    return (fs::path(directory) / name).wstring();
}

std::expected<hidhide::diag::TraceStatusResponse, hidhide::diag::ApiError> DiagnosticsTraceService::GetStatus()
                                                              const
{
    std::lock_guard lock(_mutex);

    hidhide::diag::TraceStatusResponse r;
    r.state = _state;
    r.startedAtEpoch = _startedAtEpoch;
    r.stoppedAtEpoch = _stoppedAtEpoch;
    if (!_suggestedFileName.empty())
        r.suggestedFileName = WideToUtf8(_suggestedFileName);

    return r;
}

std::expected<void, hidhide::diag::ApiError> DiagnosticsTraceService::Start(
    const hidhide::diag::TraceStartRequest& req)
{
    std::lock_guard lock(_mutex);

    if (_state == hidhide::diag::TraceSessionState::Recording)
    {
        return std::unexpected(MakeApiError("recording_in_progress",
                                            "A trace recording is already in progress."));
    }

    if (_state == hidhide::diag::TraceSessionState::Ready)
    {
        if (!_etlPath.empty())
        {
            std::error_code ec;
            fs::remove(fs::path(_etlPath), ec);
            if (ec)
                spdlog::warn("Could not remove previous .etl before new start: {}", ec.message());
        }
        _stoppedAtEpoch.reset();
        _suggestedFileName.clear();
        _state = hidhide::diag::TraceSessionState::Idle;
    }

    _scope = req.scope;
    _tuneChannels = req.tuneChannels;
    _didTuneChannels = false;

    const std::wstring dir = ResolveOutputDirectory();
    _etlPath = MakeUniqueEtlPath(dir);
    const fs::path p(_etlPath);
    _suggestedFileName = p.filename().wstring();

    if (_tuneChannels)
    {
        TuneScopeChannels(_scope, false);
        _didTuneChannels = true;
    }

    if (const auto startResult = _trace.Start(_etlPath, _scope); !startResult)
    {
        if (_didTuneChannels)
            TuneScopeChannels(_scope, true);

        _didTuneChannels = false;
        _etlPath.clear();
        _suggestedFileName.clear();

        return std::unexpected(MakeApiError(
            "trace_start_failed",
            std::string("Could not start trace session: ") + startResult.error().getErrorMessageA()));
    }

    _startedAtEpoch = NowEpochSeconds();
    _stoppedAtEpoch.reset();
    _state = hidhide::diag::TraceSessionState::Recording;

    return {};
}

std::expected<hidhide::diag::TraceStopResponse, hidhide::diag::ApiError> DiagnosticsTraceService::Stop()
{
    std::lock_guard lock(_mutex);

    if (_state != hidhide::diag::TraceSessionState::Recording)
    {
        return std::unexpected(
            MakeApiError("not_recording", "No trace recording is active."));
    }

    if (const auto stopResult = _trace.Stop(); !stopResult)
    {
        return std::unexpected(MakeApiError(
            "trace_stop_failed",
            std::string("Could not stop trace session: ") + stopResult.error().getErrorMessageA()));
    }

    if (_didTuneChannels && _tuneChannels)
        TuneScopeChannels(_scope, true);

    _stoppedAtEpoch = NowEpochSeconds();
    _state = hidhide::diag::TraceSessionState::Ready;

    hidhide::diag::TraceStopResponse out;
    out.suggestedFileName = WideToUtf8(_suggestedFileName);
    out.message = "Trace stopped; download the .etl file when ready.";

    return out;
}

std::expected<std::wstring, hidhide::diag::ApiError> DiagnosticsTraceService::GetEtlPathForDownload() const
{
    std::lock_guard lock(_mutex);

    if (_state != hidhide::diag::TraceSessionState::Ready)
    {
        return std::unexpected(
            MakeApiError("not_ready", "Trace capture is not ready for download. Stop recording first."));
    }

    if (_etlPath.empty())
        return std::unexpected(MakeApiError("missing_file", "Capture file path is unknown."));

    if (!fs::exists(fs::path(_etlPath)))
        return std::unexpected(MakeApiError("missing_file", "Capture file is no longer on disk."));

    return _etlPath;
}

std::expected<void, hidhide::diag::ApiError> DiagnosticsTraceService::DiscardCapture()
{
    std::lock_guard lock(_mutex);

    if (_state == hidhide::diag::TraceSessionState::Recording)
    {
        if (const auto stopResult = _trace.Stop(); !stopResult)
        {
            spdlog::warn("DiscardCapture: stop failed: {}", stopResult.error().getErrorMessageA());
        }
        if (_didTuneChannels && _tuneChannels)
            TuneScopeChannels(_scope, true);
    }

    if (!_etlPath.empty())
    {
        std::error_code ec;
        fs::remove(fs::path(_etlPath), ec);
        if (ec)
            spdlog::warn("DiscardCapture: delete etl failed: {}", ec.message());
    }

    _etlPath.clear();
    _suggestedFileName.clear();
    _startedAtEpoch.reset();
    _stoppedAtEpoch.reset();
    _didTuneChannels = false;
    _state = hidhide::diag::TraceSessionState::Idle;

    return {};
}
