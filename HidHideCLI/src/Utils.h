// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Utils.h
#pragma once

namespace HidHide
{
    // Get the module file name
    std::filesystem::path ModuleFileName();

    // Get the command line arguments while excluding the module file name
    std::wstring CommandLineArguments();

    // Convert the error code into an error text
    std::wstring ErrorMessage(_In_ DWORD errorCode);

    // Lookup a value from the string table resource based on the resource id provided
    std::wstring StringTable(_In_ UINT stringTableResourceId);

    // Is the standard input based on console input or not (e.g. redirected from a file and/or pipe)
    bool StandardInputRedirected();

    // Convert a guid into a string
    std::wstring GuidToString(_In_ GUID const& guid);

    // Determine if the file is an application (.exe, .com, or .bin)
    bool FileIsAnApplication(_In_ std::filesystem::path const& fullyQualifiedFileName);

    // Split the string at the white-spaces
    std::vector<std::wstring> SplitStringAtWhitespaces(_In_ std::wstring const& value);

    // Trim whitespaces at left and right
    std::wstring& Trim(_Inout_ std::wstring& value);

    // Convert a string list into a string set
    std::set<std::wstring> StringListToStringSet(_In_ std::vector<std::wstring> const& strings);

    // Convert a string list into a path set
    std::set<std::filesystem::path> StringListToPathSet(_In_ std::vector<std::wstring> const& paths);

    // Convert a string set into a string list
    std::vector<std::wstring> StringSetToStringList(_In_ std::set<std::wstring> const& strings);

    // Convert a path set into a string list
    std::vector<std::wstring> PathSetToStringList(_In_ std::set<std::filesystem::path> const& paths);

    // Convert a multi-string into a list of strings
    std::vector<std::wstring> MultiStringToStringList(_In_ std::vector<WCHAR> const& multiString);

    // Convert a set of strings into a multi-string
    std::vector<WCHAR> StringListToMultiString(_In_ std::vector<std::wstring> const& strings);

    // Get the logical file name that is being dragged
    // Returns an empty set when the data dragged isn't a list of files
    std::set<std::filesystem::path> DragTargetFileNames(_In_ COleDataObject* pDataObject);

    // Determine if the key state matches that of a copy operation (left mouse button + with or without control)
    constexpr bool DragTargetCopyOperation(_In_ DWORD keyState) noexcept
    {
        return ((0 != (MK_LBUTTON & keyState)) && (0 == (MK_MBUTTON & keyState)) && (0 == (MK_RBUTTON & keyState)) && (0 == (MK_SHIFT & keyState)) && (0 == (MK_ALT & keyState)));
    }
}
