#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hidhide::diag
{
    enum class TraceScope
    {
        Full,
        Driver,
        Userspace
    };

    enum class TraceSessionState
    {
        Idle,
        Recording,
        Ready
    };

    struct TraceStartRequest
    {
        TraceScope scope = TraceScope::Full;
        bool tuneChannels = true;
    };

    struct TraceStatusResponse
    {
        TraceSessionState state = TraceSessionState::Idle;
        std::optional<std::int64_t> startedAtEpoch;
        std::optional<std::int64_t> stoppedAtEpoch;
        std::optional<std::string> suggestedFileName;
        std::optional<std::string> message;
    };

    struct TraceStopResponse
    {
        std::string suggestedFileName;
        std::optional<std::string> message;
    };

    struct ApiError
    {
        std::string code;
        std::string message;
        std::optional<unsigned long> win32;
    };

    std::optional<TraceScope> ParseScope(std::string_view value);
}
