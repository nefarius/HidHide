// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Utils.cpp
#include "stdafx.h"
#include "Utils.h"
#include "Logging.h"

namespace HidHide
{
    std::filesystem::path ModuleFileName()
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (FALSE == ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()))) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    std::wstring CommandLineArguments()
    {
        TRACE_ALWAYS(L"");
        if (std::wstring commandLine{ ::GetCommandLineW() }; (L'"' == commandLine.at(0)))
        {
            auto const index{ commandLine.find(L'"', 1) };
            return (Trim(commandLine.erase(0, (index + 1))));
        }
        else
        {
            auto const index{ commandLine.find(L' ') };
            return (Trim(commandLine.erase(0, index)));
        }
    }

    _Use_decl_annotations_
    std::wstring ErrorMessage(_In_ DWORD errorCode)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        ::FormatMessageW((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), nullptr, errorCode, LANG_USER_DEFAULT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
        return (buffer.data());
    }

    _Use_decl_annotations_
    std::wstring StringTable(UINT stringTableResourceId)
    {
        TRACE_ALWAYS(L"");
        auto const hInstance{ ::GetModuleHandleW(nullptr) };
        if (nullptr == hInstance) THROW_WIN32_LAST_ERROR;
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        // The method also returns zero when the string is retrieved by empty so before concluding there is an error be sure to check GetLastError for details
        if ((0 == ::LoadStringW(hInstance, stringTableResourceId, buffer.data(), static_cast<int>(buffer.size()))) && (ERROR_SUCCESS != ::GetLastError())) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    bool StandardInputRedirected()
    {
        TRACE_ALWAYS(L"");
        auto const stdInputHandle{ ::GetStdHandle(STD_INPUT_HANDLE) };
        if (INVALID_HANDLE_VALUE == stdInputHandle) THROW_WIN32_LAST_ERROR;
        auto const fileType{ ::GetFileType(stdInputHandle) };
        if ((FILE_TYPE_UNKNOWN == fileType) && (NO_ERROR != ::GetLastError())) THROW_WIN32_LAST_ERROR;
        return (FILE_TYPE_CHAR != fileType);
    }

    _Use_decl_annotations_
    std::wstring GuidToString(_In_ GUID const& guid)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(39);
        if (0 == ::StringFromGUID2(guid, buffer.data(), static_cast<int>(buffer.size()))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer.data());
    }

    _Use_decl_annotations_
    std::vector<std::wstring> SplitStringAtWhitespaces(_In_ std::wstring const& value)
    {
        TRACE_ALWAYS(L"");
        std::vector<std::wstring> result;
        for (size_t begin{};;)
        {
            if (auto const position{ value.find(L' ', begin) }; (std::wstring::npos == position))
            {
                if (begin < value.size()) result.emplace_back(value.substr(begin));
                break;
            }
            else
            {
                if (begin != position) result.emplace_back(value.substr(begin, position - begin));
                begin = position + 1;
            }
        }
        return (result);
    }

    _Use_decl_annotations_
    std::wstring& Trim(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        value.erase(std::begin(value), std::find_if(std::begin(value), std::end(value), [](wchar_t character) { return (!std::isspace(character)); }));
        value.erase(std::find_if(std::rbegin(value), std::rend(value), [](wchar_t character) { return (!std::isspace(character)); }).base(), std::end(value));
        return (value);
    }

    _Use_decl_annotations_
    std::set<std::wstring> StringListToStringSet(_In_ std::vector<std::wstring> const& strings)
    {
        TRACE_ALWAYS(L"");
        std::set<std::wstring> result;
        for (auto const& string : strings)
        {
            result.emplace(string);
        }
        return (result);
    }

    _Use_decl_annotations_
    std::set<std::filesystem::path> StringListToPathSet(_In_ std::vector<std::wstring> const& paths)
    {
        TRACE_ALWAYS(L"");
        std::set<std::filesystem::path> result;
        for (auto const& path : paths)
        {
            result.emplace(path);
        }
        return (result);
    }

    _Use_decl_annotations_
    std::vector<std::wstring> StringSetToStringList(_In_ std::set<std::wstring> const& strings)
    {
        TRACE_ALWAYS(L"");
        std::vector<std::wstring> result;
        for (auto const& string : strings)
        {
            result.emplace_back(string);
        }
        return (result);
    }

    _Use_decl_annotations_
    std::vector<std::wstring> PathSetToStringList(_In_ std::set<std::filesystem::path> const& paths)
    {
        TRACE_ALWAYS(L"");
        std::vector<std::wstring> result;
        for (auto const& path : paths)
        {
            result.emplace_back(path);
        }
        return (result);
    }

    _Use_decl_annotations_
    std::vector<std::wstring> MultiStringToStringList(_In_ std::vector<WCHAR> const& multiString)
    {
        TRACE_ALWAYS(L"");
        std::vector<std::wstring> result;
        for (size_t index{}, start{}, size{ multiString.size() }; (index < size); index++)
        {
            // Skip empty strings (typically the list terminator)
            if (0 == multiString.at(index))
            {
                std::wstring const string(&multiString.at(start), 0, index - start);
                if (!string.empty()) result.emplace_back(string);
                start = index + 1;
            }
        }
        return (result);
    }

    _Use_decl_annotations_
    std::vector<WCHAR> StringListToMultiString(_In_ std::vector<std::wstring> const& strings)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> result;
        for (auto const& string : strings)
        {
            auto const oldSize{ result.size() };
            auto const appendSize{ string.size() + 1 };
            result.resize(oldSize + appendSize);
            if (0 != ::wcsncpy_s(&result.at(oldSize), appendSize, string.c_str(), appendSize)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        }
        result.push_back(L'\0');
        return (result);
    }
}
