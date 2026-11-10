// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// FilterDriverProxy.h
#pragma once

namespace HidHide
{
    typedef std::wstring DeviceInstancePath;
    typedef std::set<DeviceInstancePath> DeviceInstancePaths;
    typedef std::filesystem::path FullImageName;
    typedef std::set<FullImageName> FullImageNames;

    class FilterDriverProxy
    {
    public:

        FilterDriverProxy() noexcept = delete;
        FilterDriverProxy(_In_ FilterDriverProxy const& rhs) = delete;
        FilterDriverProxy(_In_ FilterDriverProxy&& rhs) noexcept = delete;
        FilterDriverProxy& operator=(_In_ FilterDriverProxy const& rhs) = delete;
        FilterDriverProxy& operator=(_In_ FilterDriverProxy&& rhs) = delete;

        // Exclusively lock the device driver, fill the cache layer, and ensure the module file name is always on the whitelist
        explicit FilterDriverProxy(_In_ bool writeThrough);
        ~FilterDriverProxy() = default;

        // Get the control device state
        // Returns ERROR_SUCCESS when available for use
        // Returns FILE_NOT_FOUND when the device is disabled (assuming it is installed)
        // Returns ACCESS_DENIED when in use (assuming it is not an ACL issue)
        static DWORD DeviceStatus();

        // Apply the configuration changes (if any)
        // Throws when the class is using write-through
        void ApplyConfigurationChanges();

        // Get the device Instance Paths of the Human Interface Devices that are on the black-list (may reference not present devices)
        DeviceInstancePaths GetBlacklist() const;

        // Set the device Instance Paths of the Human Interface Devices that are on the black-list
        void SetBlacklist(_In_ DeviceInstancePaths const& deviceInstancePaths);

        // Add a device to the black-list
        void BlacklistAddEntry(_In_ DeviceInstancePath const& deviceInstancePath);

        // Delete a device from the white-list
        void BlacklistDelEntry(_In_ DeviceInstancePath const& deviceInstancePath);

        // Get the applications on the white-list
        FullImageNames GetWhitelist() const;

        // Set the applications on the white-list
        void SetWhitelist(_In_ FullImageNames const& fullImageNames);

        // Add an application to the white-list
        void WhitelistAddEntry(_In_ FullImageName const& fullImageName);

        // Delete an application from the white-list
        void WhitelistDelEntry(_In_ FullImageName const& fullImageName);

        // Get the current enabled state; returns true when the device is active in hiding devices on the black-list
        bool GetActive() const;

        // Set the current enabled state
        void SetActive(_In_ bool active);

        // Get the current whitelist inverse state; returns true when the whitelist logic is the inverse (effectively an application backlist)
        bool GetInverse() const;

        // Set the current whitelist inverse state
        void SetInverse(_In_ bool inverse);

    private:

        typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> CloseHandlePtr;

        bool const           m_WriteThrough; // Flag indicating that changes should be applied instantly
        CloseHandlePtr const m_Device;       // The handle to the filter driver
        bool                 m_Active;       // Indicates if the filter driver is hiding devices or not
        DeviceInstancePaths  m_Blacklist;    // The device instance paths of the blacklisted HID devices
        FullImageNames       m_Whitelist;    // The full image names of the whitelisted applications
        bool                 m_Inverse;      // Indicates if the inverse whitelist is enabled
    };
}
