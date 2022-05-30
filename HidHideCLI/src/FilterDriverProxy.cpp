// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// FilterDriverProxy.cpp
#include "stdafx.h"
#include "FilterDriverProxy.h"
#include "Utils.h"
#include "Volume.h"
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
    constexpr auto IOCTL_GET_WLINVERSE { CTL_CODE(IoControlDeviceType, 2054, METHOD_BUFFERED, FILE_READ_DATA) };
    constexpr auto IOCTL_SET_WLINVERSE { CTL_CODE(IoControlDeviceType, 2055, METHOD_BUFFERED, FILE_READ_DATA) };

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
    HidHide::DeviceInstancePaths GetBlacklist(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_BLACKLIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_BLACKLIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (HidHide::StringListToStringSet(HidHide::MultiStringToStringList(buffer)));
    }

    // Set the device Instance Paths of the Human Interface Devices that are on the black-list
    void SetBlacklist(_In_ HANDLE device, _In_ HidHide::DeviceInstancePaths const& deviceInstancePaths)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ HidHide::StringListToMultiString(HidHide::StringSetToStringList(deviceInstancePaths)) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_BLACKLIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    // Get the applications on the white-list
    HidHide::FullImageNames GetWhitelist(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_WHITELIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_WHITELIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (HidHide::StringListToPathSet(HidHide::MultiStringToStringList(buffer)));
    }

    // Set the applications on the white-list
    void SetWhitelist(_In_ HANDLE device, _In_ HidHide::FullImageNames const& fullImageNames)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ HidHide::StringListToMultiString(HidHide::PathSetToStringList(fullImageNames)) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_WHITELIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    // Get the current whitelist inverse state; returns true when the whitelist logic is the inverse (effectively an application backlist)
    bool GetInverse(_In_ HANDLE device)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ std::vector<BOOLEAN>(1) };
        if (FALSE == ::DeviceIoControl(device, IOCTL_GET_WLINVERSE, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        if (sizeof(BOOLEAN) != needed) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (FALSE != buffer.at(0));
    }

    // Set the current whitelist inverse state
    void SetInverse(_In_ HANDLE device, _In_ bool inverse)
    {
        TRACE_ALWAYS(L"");
        DWORD needed{};
        auto buffer{ std::vector<BOOLEAN>(1) };
        buffer.at(0) = (inverse ? TRUE : FALSE);
        if (FALSE == ::DeviceIoControl(device, IOCTL_SET_WLINVERSE, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }
}

namespace HidHide
{
    _Use_decl_annotations_
    FilterDriverProxy::FilterDriverProxy(bool writeThrough)
        : m_WriteThrough{ writeThrough }
        , m_Device{ ::Device(HidHide::StringTable(IDS_CONTROL_DEVICE_NAME)) }
        , m_Active{ ::GetActive(m_Device.get()) }
        , m_Blacklist{ ::GetBlacklist(m_Device.get()) }
        , m_Whitelist{ ::GetWhitelist(m_Device.get()) }
        , m_Inverse{ ::GetInverse(m_Device.get()) }
    {
        TRACE_ALWAYS(L"");

        if (auto const fullImageName{ HidHide::FileNameToFullImageName(HidHide::ModuleFileName()) }; !fullImageName.empty())
        {
            // Ensure the application itself is always on the whitelist if inverse whitelist is off or always off
            // the whitelist if inverse is on and apply the change immediately
            if ((!m_Inverse && m_Whitelist.emplace(fullImageName).second) || (m_Inverse && m_Whitelist.erase(fullImageName)))
                ::SetWhitelist(m_Device.get(), m_Whitelist);
        }
    }

    DWORD FilterDriverProxy::DeviceStatus()
    {
        TRACE_ALWAYS(L"");
        auto const handle{ CloseHandlePtr(::CreateFileW(HidHide::StringTable(IDS_CONTROL_DEVICE_NAME).c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), &::CloseHandle) };
        if ((INVALID_HANDLE_VALUE == handle.get()) && (ERROR_ACCESS_DENIED != ::GetLastError()) && (ERROR_FILE_NOT_FOUND != ::GetLastError())) THROW_WIN32_LAST_ERROR;
        return ((INVALID_HANDLE_VALUE == handle.get()) ? ::GetLastError() : ERROR_SUCCESS);
    }

    void FilterDriverProxy::ApplyConfigurationChanges()
    {
        TRACE_ALWAYS(L"");
        if (m_WriteThrough) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (::GetWhitelist(m_Device.get()) != m_Whitelist) ::SetWhitelist(m_Device.get(), m_Whitelist);
        if (::GetBlacklist(m_Device.get()) != m_Blacklist) ::SetBlacklist(m_Device.get(), m_Blacklist);
        if (::GetActive(m_Device.get()) != m_Active) ::SetActive(m_Device.get(), m_Active);
        if (::GetInverse(m_Device.get()) != m_Inverse) ::SetInverse(m_Device.get(), m_Inverse);
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
        if (m_Active != active)
        {
            m_Active = active;
            if (m_WriteThrough) ::SetActive(m_Device.get(), m_Active);
        }
    }

    DeviceInstancePaths FilterDriverProxy::GetBlacklist() const
    {
        TRACE_ALWAYS(L"");
        return (m_Blacklist);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetBlacklist(DeviceInstancePaths const& deviceInstancePaths)
    {
        TRACE_ALWAYS(L"");
        if (m_Blacklist != deviceInstancePaths)
        {
            m_Blacklist = deviceInstancePaths;
            if (m_WriteThrough) ::SetBlacklist(m_Device.get(), m_Blacklist);
        }
    }

    _Use_decl_annotations_
    void FilterDriverProxy::BlacklistAddEntry(DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        if (m_Blacklist.emplace(deviceInstancePath).second)
        {
            if (m_WriteThrough) ::SetBlacklist(m_Device.get(), m_Blacklist);
        }
    }

    _Use_decl_annotations_
    void FilterDriverProxy::BlacklistDelEntry(DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        if (auto const it{ m_Blacklist.find(deviceInstancePath) }; std::end(m_Blacklist) != it)
        {
            m_Blacklist.erase(it);
            if (m_WriteThrough) ::SetBlacklist(m_Device.get(), m_Blacklist);
        }
    }

    FullImageNames FilterDriverProxy::GetWhitelist() const
    {
        TRACE_ALWAYS(L"");
        return (m_Whitelist);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetWhitelist(FullImageNames const& fullImageNames)
    {
        TRACE_ALWAYS(L"");
        if (m_Whitelist != fullImageNames)
        {
            m_Whitelist = fullImageNames;
            if (m_WriteThrough) ::SetWhitelist(m_Device.get(), m_Whitelist);
        }
    }

    _Use_decl_annotations_
    void FilterDriverProxy::WhitelistAddEntry(FullImageName const& fullImageName)
    {
        TRACE_ALWAYS(L"");
        if (m_Whitelist.emplace(fullImageName).second)
        {
            if (m_WriteThrough) ::SetWhitelist(m_Device.get(), m_Whitelist);
        }
    }

    _Use_decl_annotations_
    void FilterDriverProxy::WhitelistDelEntry(FullImageName const& fullImageName)
    {
        TRACE_ALWAYS(L"");
        if (auto const it{ m_Whitelist.find(fullImageName) }; std::end(m_Whitelist) != it)
        {
            m_Whitelist.erase(it);
            if (m_WriteThrough) ::SetWhitelist(m_Device.get(), m_Whitelist);
        }
    }

    bool FilterDriverProxy::GetInverse() const
    {
        TRACE_ALWAYS(L"");
        return (m_Inverse);
    }

    _Use_decl_annotations_
    void FilterDriverProxy::SetInverse(bool inverse)
    {
        TRACE_ALWAYS(L"");
        if (m_Inverse != inverse)
        {
            m_Inverse = inverse;
            if (m_WriteThrough) ::SetInverse(m_Device.get(), m_Inverse);
        }
    }
}