#include "ETWUtil.h"

#include <scope_guard.hpp>

#include <winevt.h>
#include <strsafe.h>

using namespace nefarius::utilities;


std::expected<void, Win32Error> etw::utils::SetLogEnabled(LPCWSTR channel, const bool enabled)
{
    EVT_HANDLE config = EvtOpenChannelConfig(nullptr, channel, 0);
    if (!config)
    {
        return std::unexpected(Win32Error("EvtOpenChannelConfig"));
    }

    const auto guard = sg::make_scope_guard(
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
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty EvtChannelConfigEnabled"));
    }

    if (!EvtSaveChannelConfig(config, 0))
    {
        return std::unexpected(Win32Error("EvtSaveChannelConfig"));
    }

    return {};
}

std::expected<void, Win32Error> etw::utils::SetDiagnosticChannelPublishingKeywords(
    LPCWSTR channel,
    const bool enabled,
    const ULONGLONG publishingKeywords)
{
    EVT_HANDLE h = EvtOpenChannelConfig(nullptr, channel, 0);
    if (!h)
    {
        return std::unexpected(Win32Error("EvtOpenChannelConfig"));
    }

    const auto guard = sg::make_scope_guard(
        [ h ]() noexcept
        {
            if (h)
                EvtClose(h);
        });

    EVT_VARIANT off = {};
    off.Type = EvtVarTypeBoolean;
    off.BooleanVal = FALSE;
    if (!EvtSetChannelConfigProperty(h, EvtChannelConfigEnabled, 0, &off))
    {
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty disable"));
    }

    EVT_VARIANT keywords = {};
    keywords.Type = EvtVarTypeUInt64;
    keywords.UInt64Val = publishingKeywords;
    if (!EvtSetChannelConfigProperty(h, EvtChannelPublishingConfigKeywords, 0, &keywords))
    {
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty Keywords"));
    }

    EVT_VARIANT on = {};
    on.Type = EvtVarTypeBoolean;
    on.BooleanVal = enabled ? TRUE : FALSE;
    if (!EvtSetChannelConfigProperty(h, EvtChannelConfigEnabled, 0, &on))
    {
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty enable"));
    }

    if (!EvtSaveChannelConfig(h, 0))
    {
        return std::unexpected(Win32Error("EvtSaveChannelConfig"));
    }

    return {};
}

std::expected<void, Win32Error> etw::utils::ApplyReadmeMaximumDiagnosticVisibility(LPCWSTR channel)
{
    return SetDiagnosticChannelPublishingKeywords(channel, true, 5ull);
}

std::expected<void, Win32Error> etw::utils::RestoreReadmeDefaultDiagnosticVisibility(LPCWSTR channel)
{
    return SetDiagnosticChannelPublishingKeywords(channel, true, 1ull);
}
