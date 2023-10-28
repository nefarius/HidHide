#include "Util.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <winreg/WinReg.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>


bool util::add_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                   DeviceClassFilterPosition::Value position)
{
    auto key = SetupDiOpenClassRegKey(classGuid, KEY_ALL_ACCESS);

    if (INVALID_HANDLE_VALUE == key)
    {
        spdlog::error("SetupDiOpenClassRegKey failed with error code %v", GetLastError());
        return false;
    }

    LPCWSTR filterValue = (position == DeviceClassFilterPosition::Lower) ? L"LowerFilters" : L"UpperFilters";
    DWORD type, size;
    std::vector<std::wstring> filters;

    auto status = RegQueryValueExW(
        key,
        filterValue,
        nullptr,
        &type,
        nullptr,
        &size
    );

    //
    // Value exists already, read it with returned buffer size
    // 
    if (status == ERROR_SUCCESS)
    {
        std::vector<wchar_t> temp(size / sizeof(wchar_t));

        status = RegQueryValueExW(
            key,
            filterValue,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(&temp[0]),
            &size
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegQueryValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        size_t index = 0;
        size_t len = wcslen(&temp[0]);
        while (len > 0)
        {
            filters.emplace_back(&temp[index]);
            index += len + 1;
            len = wcslen(&temp[index]);
        }

        //
        // Filter not there yet, add
        // 
        if (std::find(filters.begin(), filters.end(), filterName) == filters.end())
        {
            filters.emplace_back(filterName);
        }

        const std::vector<wchar_t> multiString = winreg::detail::BuildMultiString(filters);

        const auto& dataSize = multiString.size() * sizeof(wchar_t);

        status = RegSetValueExW(
            key,
            filterValue,
            0, // reserved
            REG_MULTI_SZ,
            reinterpret_cast<const BYTE*>(&multiString[0]),
            static_cast<DWORD>(dataSize)
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegSetValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        RegCloseKey(key);
        return true;
    }
    //
    // Value doesn't exist, create and populate
    // 
    if (status == ERROR_FILE_NOT_FOUND)
    {
        filters.emplace_back(filterName);

        const std::vector<wchar_t> multiString = winreg::detail::BuildMultiString(filters);

        const auto dataSize = multiString.size() * sizeof(wchar_t);

        status = RegSetValueExW(
            key,
            filterValue,
            0, // reserved
            REG_MULTI_SZ,
            reinterpret_cast<const BYTE*>(&multiString[0]),
            (DWORD)dataSize
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegSetValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        RegCloseKey(key);
        return true;
    }

    RegCloseKey(key);
    return false;
}

bool util::remove_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                      DeviceClassFilterPosition::Value position)
{
    auto key = SetupDiOpenClassRegKey(classGuid, KEY_ALL_ACCESS);

    if (INVALID_HANDLE_VALUE == key)
    {
        spdlog::error("SetupDiOpenClassRegKey failed with error code %v", GetLastError());
        return false;
    }

    LPCWSTR filterValue = (position == DeviceClassFilterPosition::Lower) ? L"LowerFilters" : L"UpperFilters";
    DWORD type, size;
    std::vector<std::wstring> filters;

    auto status = RegQueryValueExW(
        key,
        filterValue,
        nullptr,
        &type,
        nullptr,
        &size
    );

    //
    // Value exists already, read it with returned buffer size
    // 
    if (status == ERROR_SUCCESS)
    {
        std::vector<wchar_t> temp(size / sizeof(wchar_t));

        status = RegQueryValueExW(
            key,
            filterValue,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(&temp[0]),
            &size
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegQueryValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        //
        // Remove value, if found
        //
        size_t index = 0;
        size_t len = wcslen(&temp[0]);
        while (len > 0)
        {
            if (filterName != &temp[index])
            {
                filters.emplace_back(&temp[index]);
            }
            index += len + 1;
            len = wcslen(&temp[index]);
        }

        const std::vector<wchar_t> multiString = winreg::detail::BuildMultiString(filters);

        const auto dataSize = multiString.size() * sizeof(wchar_t);

        status = RegSetValueExW(
            key,
            filterValue,
            0, // reserved
            REG_MULTI_SZ,
            reinterpret_cast<const BYTE*>(&multiString[0]),
            (DWORD)dataSize
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegSetValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        RegCloseKey(key);
        return true;
    }
    //
    // Value doesn't exist, return
    // 
    if (status == ERROR_FILE_NOT_FOUND)
    {
        RegCloseKey(key);
        return true;
    }

    RegCloseKey(key);
    return false;
}

bool util::has_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                   DeviceClassFilterPosition::Value position, bool& found)
{
    const auto key = SetupDiOpenClassRegKey(classGuid, KEY_READ);

    if (INVALID_HANDLE_VALUE == key)
    {
        spdlog::error("SetupDiOpenClassRegKey failed with error code %v", GetLastError());
        return false;
    }

    LPCWSTR filterValue = (position == DeviceClassFilterPosition::Lower) ? L"LowerFilters" : L"UpperFilters";
    DWORD type, size;
    std::vector<std::wstring> filters;

    auto status = RegQueryValueExW(
        key,
        filterValue,
        nullptr,
        &type,
        nullptr,
        &size
    );

    //
    // Value exists already, read it with returned buffer size
    // 
    if (status == ERROR_SUCCESS)
    {
        std::vector<wchar_t> temp(size / sizeof(wchar_t));

        status = RegQueryValueExW(
            key,
            filterValue,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(&temp[0]),
            &size
        );

        if (status != ERROR_SUCCESS)
        {
            spdlog::error("RegQueryValueExW failed with status %v", status);
            RegCloseKey(key);
            SetLastError(status);
            return false;
        }

        //
        // Enumerate values
        //
        size_t index = 0;
        size_t len = wcslen(&temp[0]);
        while (len > 0)
        {
            if (filterName == &temp[index])
            {
                found = true;
                break;
            }
            index += len + 1;
            len = wcslen(&temp[index]);
        }

        RegCloseKey(key);
        return true;
    }
    //
    // Value doesn't exist, return
    // 
    if (status == ERROR_FILE_NOT_FOUND)
    {
        RegCloseKey(key);
        return true;
    }

    RegCloseKey(key);
    return false;
}
