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
}
