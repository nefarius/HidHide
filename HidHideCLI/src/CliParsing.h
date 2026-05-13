// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// CliParsing.h
#pragma once

#include <string>
#include <vector>

namespace HidHide::CliParsing
{
    using Args = std::vector<std::wstring>;

    std::wstring& Trim(std::wstring& value);

    std::wstring ExtractKeyword(std::wstring& value);

    std::wstring ExtractString(std::wstring& value);

    Args ExtractCommand(std::wstring& value);

    std::vector<Args> ExtractCommands(std::wstring& value);
}
