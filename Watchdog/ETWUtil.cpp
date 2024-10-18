#include "ETWUtil.h"

#include <scope_guard.hpp>

#include <winevt.h>

using namespace nefarius::utilities;


std::expected<void, Win32Error> etw::utils::SetLogEnabled(LPCWSTR channel, bool enabled)
{
    EVT_HANDLE config = EvtOpenChannelConfig(NULL, channel, 0);
    if (!config)
    {
        return std::unexpected(Win32Error("EvtOpenChannelConfig"));
    }

    auto guard = sg::make_scope_guard(
        [ config ]() noexcept
        {
            if (config)
                EvtClose(config);
        });

    EVT_VARIANT logEnabled = {};
    logEnabled.Type = EvtVarTypeBoolean;
    logEnabled.BooleanVal = enabled;

    if (!EvtSetChannelConfigProperty(config, EvtChannelConfigEnabled, 0, &logEnabled))
    {
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty"));
    }

    if (!EvtSaveChannelConfig(config, 0))
    {
        return std::unexpected(Win32Error("EvtSaveChannelConfig"));
    }

    return {};
}
