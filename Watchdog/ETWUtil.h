#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <expected>
#include <string>
#include <format>

#include <nefarius/neflib/Win32Error.hpp>

namespace etw::utils
{
    std::expected<void, nefarius::utilities::Win32Error> SetLogEnabled(LPCWSTR channel, bool enabled);

    //
    // Matches README.md wevtutil sequence for analytic Diagnostic channels:
    // disable → set publishing keywords → enable → save.
    // maxKeywords defaults to 5 and restoreKeywords to 1 (decimal), per README examples.
    //
    std::expected<void, nefarius::utilities::Win32Error> SetDiagnosticChannelPublishingKeywords(
        LPCWSTR channel,
        bool enabled,
        ULONGLONG publishingKeywords);

    std::expected<void, nefarius::utilities::Win32Error> ApplyReadmeMaximumDiagnosticVisibility(LPCWSTR channel);

    std::expected<void, nefarius::utilities::Win32Error> RestoreReadmeDefaultDiagnosticVisibility(LPCWSTR channel);
}
