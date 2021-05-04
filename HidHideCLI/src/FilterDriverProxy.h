// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// FilterDriverProxy.h
#pragma once

namespace HidHide
{
    class FilterDriverProxy
    {
    public:

        typedef std::wstring DeviceInstancePath;
        typedef std::set<DeviceInstancePath> DeviceInstancePaths;
        typedef std::filesystem::path FullImageName;
        typedef std::set<FullImageName> FullImageNames;

        FilterDriverProxy() noexcept = delete;
        FilterDriverProxy(_In_ FilterDriverProxy const& rhs) = delete;
        FilterDriverProxy(_In_ FilterDriverProxy&& rhs) noexcept = delete;
        FilterDriverProxy& operator=(_In_ FilterDriverProxy const& rhs) = delete;
        FilterDriverProxy& operator=(_In_ FilterDriverProxy&& rhs) = delete;

        explicit FilterDriverProxy(_In_ std::filesystem::path const& deviceName);
        ~FilterDriverProxy() = default;

        // Add an entry to the whitelist and immediately apply the change
        static void WhitelistAddEntry(_In_ std::filesystem::path const& deviceName, _In_ FullImageName const& fullImageName);

        // Apply the configuration changes (if any)
        void ApplyConfigurationChanges();

        // Get the control device state; returns true when the control device is present (installed and enabled)
        bool Present() const;

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

    private:

        typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> CloseHandlePtr;

        CloseHandlePtr const m_Device;    // The handle to the filter driver
        bool                 m_Active;    // Indicates if the filter driver is hiding devices or not
        DeviceInstancePaths  m_Blacklist; // The device instance paths of the blacklisted HID devices
        FullImageNames       m_Whitelist; // The full image names of the whitelisted applications
    };
}
