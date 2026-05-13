// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// CliParsing.cpp
#include "CliParsing.h"

#include <algorithm>
#include <cwctype>
#include <string>

namespace HidHide::CliParsing
{
    std::wstring& Trim(std::wstring& value)
    {
        value.erase(std::begin(value), std::find_if(std::begin(value), std::end(value), [](wchar_t character) { return (!std::iswspace(character)); }));
        value.erase(std::find_if(std::rbegin(value), std::rend(value), [](wchar_t character) { return (!std::iswspace(character)); }).base(), std::end(value));
        return (value);
    }

    std::wstring ExtractKeyword(std::wstring& value)
    {
        if (auto const it{ std::find_if(std::begin(value), std::end(value), [](wchar_t character) { return (std::iswspace(character)); }) }; (std::end(value) != it))
        {
            auto const result{ std::wstring(std::begin(value), it) };
            value.erase(std::begin(value), it + 1);
            return (result);
        }

        auto const result{ value };
        value.clear();
        return (result);
    }

    std::wstring ExtractString(std::wstring& value)
    {
        if (value.empty()) return (std::wstring{});
        if (L'"' == value.at(0))
        {
            auto const index{ value.find(L'"', 1) };
            auto const indexAtEnd{ index == (value.size() - 1) };
            if (!((std::wstring::npos != index) && (indexAtEnd || std::iswspace(value.at(index + 1))))) return (std::wstring{});
            auto const result{ std::wstring(std::begin(value) + 1, std::begin(value) + index) };
            value.erase(std::begin(value), std::begin(value) + index + (indexAtEnd ? 1 : 2));
            return (result);
        }

        return (ExtractKeyword(value));
    }

    Args ExtractCommand(std::wstring& value)
    {
        Args result{};

        if (L"--" == value.substr(0, 2)) value.erase(std::begin(value), std::begin(value) + 2);

        auto const keyword{ ExtractKeyword(value) };
        result.emplace_back(keyword);

        while ((!Trim(value).empty()) && (L"--" != value.substr(0, 2)))
        {
            auto const arg{ ExtractString(value) };
            if ((arg.empty()) && (!value.empty())) return (Args{});
            result.emplace_back(arg);
        }

        return (result);
    }

    std::vector<Args> ExtractCommands(std::wstring& value)
    {
        std::vector<Args> result{};

        while (!Trim(value).empty())
        {
            auto const command{ ExtractCommand(value) };
            if (command.empty() && (!value.empty())) return (std::vector<Args>());
            result.emplace_back(command);
        }

        return (result);
    }
}
