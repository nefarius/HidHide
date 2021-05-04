// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// FilterDriverProxy.cpp
#include "stdafx.h"
#include "FilterDriverProxy.h"
#include "Utils.h"
#include "Logging.h"

namespace
{
    typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> CloseHandlePtr;

    // The Hid Hide I/O control custom device type (range 32768 .. 65535)
    constexpr auto IoControlDeviceType{ 32769u };

    // The Hid Hide I/O control codes
    constexpr auto IOCTL_GET_WHITELIST { CTL_CODE(IoControlDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_SET_WHITELIST { CTL_CODE(IoControlDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_GET_BLACKLIST { CTL_CODE(IoControlDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_SET_BLACKLIST { CTL_CODE(IoControlDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_GET_ACTIVE    { CTL_CODE(IoControlDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_SET_ACTIVE    { CTL_CODE(IoControlDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA) };

    // Get a file handle to the device driver
    // The flag allowFileNotFound is applied when the device couldn't be found and controls whether or not an exception is thrown on failure
    CloseHandlePtr Device(_In_ std::filesystem::path const& deviceName, _In_ bool allowFileNotFound)
    {
        TRACE_ALWAYS(L"");
        auto handle{ CloseHandlePtr(::CreateFileW(deviceName.native().c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), &::CloseHandle) };
        if ((INVALID_HANDLE_VALUE == handle.get()) && ((ERROR_FILE_NOT_FOUND != ::GetLastError()) || (!allowFileNotFound))) THROW_WIN32_LAST_ERROR;
        return (handle);
    }

    // Get a file handle to the device driver; will throw when the device isn't found
    CloseHandlePtr Device(_In_ std::filesystem::path const& deviceName)
    {
        return (Device(deviceName, false));
    }

    // Get the current enabled state; returns true when the device is active in hiding devices on the black-list
    bool GetActive(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ std::vector<BOOLEAN>(1) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_ACTIVE, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        if (sizeof(BOOLEAN) != needed) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (FALSE != buffer.at(0));
    }

    // Set the current enabled state
    void SetActive(_In_ HANDLE device, _In_ bool active)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ std::vector<BOOLEAN>(1) };
        buffer.at(0) = (active ? TRUE : FALSE);
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_ACTIVE, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    // Get the device Instance Paths of the Human Interface Devices that are on the black-list (may reference not present devices)
    HidHide::FilterDriverProxy::DeviceInstancePaths GetBlacklist(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_BLACKLIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_BLACKLIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (HidHide::StringListToStringSet(HidHide::MultiStringToStringList(buffer)));
    }

    // Set the device Instance Paths of the Human Interface Devices that are on the black-list
    void SetBlacklist(_In_ HANDLE device, _In_ HidHide::FilterDriverProxy::DeviceInstancePaths const& deviceInstancePaths)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ HidHide::StringListToMultiString(HidHide::StringSetToStringList(deviceInstancePaths)) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_BLACKLIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    // Get the applications on the white-list
    HidHide::FilterDriverProxy::FullImageNames GetWhitelist(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_WHITELIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_WHITELIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (HidHide::StringListToPathSet(HidHide::MultiStringToStringList(buffer)));
    }

    // Set the applications on the white-list
    void SetWhitelist(_In_ HANDLE device, _In_ HidHide::FilterDriverProxy::FullImageNames const& fullImageNames)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ HidHide::StringListToMultiString(HidHide::PathSetToStringList(fullImageNames)) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_WHITELIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }
}

namespace HidHide
{
    _Use_decl_annotations_
    FilterDriverProxy::FilterDriverProxy(std::filesystem::path const& deviceName)
        : m_Device{ Device(deviceName) }
        , m_Active{ ::GetActive(m_Device.get()) }
        , m_Blacklist{ ::GetBlacklist(m_Device.get()) }
        , m_Whitelist{ ::GetWhitelist(m_Device.get()) }
    {
        TRACE_ALWAYS(L"");
    }

    _Use_decl_annotations_
    void FilterDriverProxy::WhitelistAddEntry(std::filesystem::path const& deviceName, FullImageName const& fullImageName)
    {
        TRACE_ALWAYS(L"");
        auto const device{ Device(deviceName) };
        if (auto whitelist{ ::GetWhitelist(device.get()) }; whitelist.emplace(fullImageName).second)
        {
            ::SetWhitelist(device.get(), whitelist);
        }
    }

    void FilterDriverProxy::ApplyConfigurationChanges()
    {
        TRACE_ALWAYS(L"");
        if (::GetWhitelist(m_Device.get()) != m_Whitelist) ::SetWhitelist(m_Device.get(), m_Whitelist);
        if (::GetBlacklist(m_Device.get()) != m_Blacklist) ::SetBlacklist(m_Device.get(), m_Blacklist);
        if (::GetActive(m_Device.get()) != m_Active) ::SetActive(m_Device.get(), m_Active);
    }

    bool FilterDriverProxy::Present() const
    {
        TRACE_ALWAYS(L"");
        return (INVALID_HANDLE_VALUE != m_Device.get());
    }

    bool FilterDriverProxy::GetActive() const
    {
        TRACE_ALWAYS(L"");
        return (m_Active);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetActive(bool active)
    {
        TRACE_ALWAYS(L"");
        m_Active = active;
    }

    FilterDriverProxy::DeviceInstancePaths FilterDriverProxy::GetBlacklist() const
    {
        TRACE_ALWAYS(L"");
        return (m_Blacklist);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetBlacklist(FilterDriverProxy::DeviceInstancePaths const& deviceInstancePaths)
    {
        TRACE_ALWAYS(L"");
        m_Blacklist = deviceInstancePaths;
    }

    _Use_decl_annotations_
    void FilterDriverProxy::BlacklistAddEntry(FilterDriverProxy::DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        m_Blacklist.emplace(deviceInstancePath);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::BlacklistDelEntry(FilterDriverProxy::DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        m_Blacklist.erase(deviceInstancePath);
    }

    FilterDriverProxy::FullImageNames FilterDriverProxy::GetWhitelist() const
    {
        TRACE_ALWAYS(L"");
        return (m_Whitelist);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetWhitelist(FilterDriverProxy::FullImageNames const& fullImageNames)
    {
        TRACE_ALWAYS(L"");
        m_Whitelist = fullImageNames;
    }

    _Use_decl_annotations_
    void FilterDriverProxy::WhitelistAddEntry(FilterDriverProxy::FullImageName const& fullImageName)
    {
        TRACE_ALWAYS(L"");
        m_Whitelist.emplace(fullImageName);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::WhitelistDelEntry(FilterDriverProxy::FullImageName const& fullImageName)
    {
        TRACE_ALWAYS(L"");
        m_Whitelist.erase(fullImageName);
    }
}
