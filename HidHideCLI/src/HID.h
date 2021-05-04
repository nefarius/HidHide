// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HID.h
#pragma once

namespace HidHide
{
    typedef std::wstring DeviceInstancePath;

    struct HidDeviceInformation
    {
        bool                  present;
        bool                  gamingDevice;
        std::filesystem::path symbolicLink;
        std::wstring          vendor;
        std::wstring          product;
        std::wstring          serialNumber;
        std::wstring          usage;
        std::wstring          description;
        DeviceInstancePath    deviceInstancePath;
        DeviceInstancePath    baseContainerDeviceInstancePath;
        GUID                  baseContainerClassGuid;
        size_t                baseContainerDeviceCount;
    };

    typedef std::multimap<std::wstring, std::vector<HidDeviceInformation>> FriendlyNamesAndHidDeviceInformation;

    // Get the device instance paths of the HID devices (present or not) and associated device information and cluster/group them on their device friendly names
    FriendlyNamesAndHidDeviceInformation HidDevices(_In_ bool gamingDevicesOnly);
}
