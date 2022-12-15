// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HID.cpp
#include "stdafx.h"
#include "HID.h"
#include "Utils.h"
#include "Logging.h"

namespace
{
    typedef HidHide::DeviceInstancePath DeviceInstancePath;
    typedef std::set< DeviceInstancePath> DeviceInstancePaths;
    typedef std::multimap<DeviceInstancePath, HidHide::HidDeviceInformation> BaseContainerDeviceInstancePathAndHidDeviceInformation;
    typedef std::function<__declspec(noreturn) bool(_In_ DeviceInstancePath const& deviceInstancePath, _In_ std::filesystem::path const& symbolicLink)> IterateAllHidDevicesPresentOrNotPresentCallback;
    typedef std::unique_ptr<std::remove_pointer<HDEVINFO>::type, decltype(&::SetupDiDestroyDeviceInfoList)> SetupDiDestroyDeviceInfoListPtr;
    typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> CloseHandlePtr;
    typedef std::unique_ptr<std::remove_pointer<PHIDP_PREPARSED_DATA>::type, decltype(&::HidD_FreePreparsedData)> HidD_FreePreparsedDataPtr;

    // The Microsoft API states that the maximum string size is device dependent. Limiting the string size to a particular device (like the USB limit)
    // may fail for other devices. Its however also observed that the Microsoft API fails when the offered string size is rather large (like UNICODE_STRING_MAX_CHARS)
    // Unfortunately, the actual max size the Microsoft API is handling doesn't seem to be specified explicitely hence we need to define it ourselfs.
    // Experimenting with some calls revealed that the max buffer size currently supported is 0xFFF in bytes.
    constexpr size_t HidApiStringSizeInCharacters{ 127 * sizeof(WCHAR) };

    // Get the Device Instance Paths of all devices of a given class (present or not present)
    // Note that performing a query for the Hid-Hide service controlled devices turned out not to work for VMWare environments hence query the whole list here and work from there
    DeviceInstancePaths DeviceInstancePathsPresentOrNot(_In_ GUID const& classGuid)
    {
        TRACE_ALWAYS(L"");
        ULONG needed{};
        if (auto const result{ ::CM_Get_Device_ID_List_SizeW(&needed, HidHide::GuidToString(classGuid).c_str(), CM_GETIDLIST_FILTER_CLASS) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        std::vector<WCHAR> buffer(needed);
        if (auto const result{ ::CM_Get_Device_ID_ListW(HidHide::GuidToString(classGuid).c_str(), buffer.data(), needed, CM_GETIDLIST_FILTER_CLASS) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        return (HidHide::StringListToStringSet(HidHide::MultiStringToStringList(buffer)));
    }

    // Determine the Symbolic Link towards a particular device
    // Returns the Symbolic Link suited for CreateFile or an empty string when the device interface isn't supported
    std::filesystem::path SymbolicLink(_In_ GUID const& deviceInterfaceGuid, _In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        // Ask the device for the device interface
        // Note that this call will succeed, whether or not the interface is present, but the iterator will have no entries, when the device interface isn't supported
        auto const handle{ SetupDiDestroyDeviceInfoListPtr(::SetupDiGetClassDevsW(&deviceInterfaceGuid, deviceInstancePath.c_str(), nullptr, DIGCF_DEVICEINTERFACE), &::SetupDiDestroyDeviceInfoList) };
        if (INVALID_HANDLE_VALUE == handle.get()) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Is the interface supported ?
        SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
        deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
        if (FALSE == ::SetupDiEnumDeviceInterfaces(handle.get(), nullptr, &deviceInterfaceGuid, 0, &deviceInterfaceData))
        {
            if (ERROR_NO_MORE_ITEMS != ::GetLastError()) THROW_WIN32_LAST_ERROR;

            // Sorry but not a device interface isn't supported
            return (std::filesystem::path{});
        }

        // Determine the buffer length needed
        DWORD needed{};
        if ((FALSE == ::SetupDiGetDeviceInterfaceDetailW(handle.get(), &deviceInterfaceData, nullptr, 0, &needed, nullptr)) && (ERROR_INSUFFICIENT_BUFFER != ::GetLastError())) THROW_WIN32_LAST_ERROR;
        std::vector<BYTE> buffer(needed);

        // Acquire the detailed data containing the symbolic link (aka. device path)
        auto& deviceInterfaceDetailData{ *reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data()) };
        deviceInterfaceDetailData.cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (FALSE == ::SetupDiGetDeviceInterfaceDetailW(handle.get(), &deviceInterfaceData, reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data()), static_cast<DWORD>(buffer.size()), nullptr, nullptr)) THROW_WIN32_LAST_ERROR;
        return (deviceInterfaceDetailData.DevicePath);
    }

    // Iterate all human interface devices found (present or not)
    void IterateAllHidDevicesPresentOrNotPresent(_In_ IterateAllHidDevicesPresentOrNotPresentCallback iterateAllHidDevicesPresentOrNotPresentCallback)
    {
        TRACE_ALWAYS(L"");

        // Get the HID interface, as used by DirectInput
        GUID hidDeviceInterfaceGuid;
        ::HidD_GetHidGuid(&hidDeviceInterfaceGuid);

        // Get the Device Instance Paths of all HID devices
        // Note that the list includes base container devices that don't offer a HID interface
        for (auto const& deviceInstancePath : DeviceInstancePathsPresentOrNot(GUID_DEVCLASS_HIDCLASS))
        {
            // Filter the list for devices with a HID interface
            if (auto const symbolicLink{ SymbolicLink(hidDeviceInterfaceGuid, deviceInstancePath) }; !symbolicLink.empty())
            {
                if (!iterateAllHidDevicesPresentOrNotPresentCallback(deviceInstancePath, symbolicLink)) return;
            }
        }
    }

    // Get the device description
    std::wstring DeviceDescription(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        DEVINST     devInst{};
        DEVPROPTYPE devPropType{};
        ULONG       needed{};
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_DeviceDesc, &devPropType, nullptr, &needed, 0) }; (CR_BUFFER_SMALL != result)) THROW_CONFIGRET(result);
        if (DEVPROP_TYPE_STRING != devPropType) THROW_WIN32(ERROR_INVALID_PARAMETER);
        std::vector<WCHAR> buffer(needed);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_DeviceDesc, &devPropType, reinterpret_cast<PBYTE>(buffer.data()), &needed, 0) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        return (buffer.data());
    }

    // Is the device present or absent
    bool DevicePresent(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        DEVINST devInst{};
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_NORMAL) }; (CR_NO_SUCH_DEVNODE == result) || (CR_SUCCESS == result)) return (CR_SUCCESS == result); else THROW_CONFIGRET(result);
    }

    // Get the usage name from the usage page and usage id
    // Based on Revision 1.2 of standard (see also https://usb.org/sites/default/files/hut1_2.pdf)
    std::wstring Usage(_In_ HIDP_CAPS const& capabilities)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);

        // For usage id 0x00 through 0xFF of page 1 we have a dedicated lookup table for the usage name at resource id offset 0x1100
        if ((0x01 == capabilities.UsagePage) && (0xFF >= capabilities.Usage) && (0 != ::LoadStringW(nullptr, (0x1100 + capabilities.Usage), buffer.data(), static_cast<int>(buffer.size())))) return (buffer.data());

        // For usage id 0x00 through 0xFF of page 12 we have a dedicated lookup table for the usage name at resource id offset 0x1200
        if ((0x0C == capabilities.UsagePage) && (0xFF >= capabilities.Usage) && (0 != ::LoadStringW(nullptr, (0x1200 + capabilities.Usage), buffer.data(), static_cast<int>(buffer.size())))) return (buffer.data());

        // For usage page 0x00 through 0xFF we have a dedicated lookup table for the page name at resource id offset 0x1000
        std::wostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << std::hex << std::uppercase << std::setfill(L'0') << std::setw(4);
        if ((0xFF >= capabilities.UsagePage) && (0 != ::LoadStringW(nullptr, (0x1000 + capabilities.UsagePage), buffer.data(), static_cast<int>(buffer.size()))))
        {
            // Found page now only show the usage id
            os << buffer.data() << L" " << capabilities.Usage;
        }
        else
        {
            // Generic catch-all showing the page and usage id
            os << capabilities.UsagePage << L" " << capabilities.Usage;
        }

        return (os.str());
    }

    // Determine from the model information and usage if this is a gaming device or not
    bool GamingDevice(_In_ HIDD_ATTRIBUTES const& attributes, _In_ HIDP_CAPS const& capabilities)
    {
        TRACE_ALWAYS(L"");

        // 0x28DE 0x1142 = Valve Corporation Steam Controller
        // 0x28DE 0x1205 = Valve Software Steam Deck Controller
        return (((attributes.VendorID == 0x28DE) && (attributes.ProductID == 0x1205)) || ((attributes.VendorID == 0x28DE) && (attributes.ProductID == 0x1142)) || (0x05 == capabilities.UsagePage) || (0x01 == capabilities.UsagePage) && ((0x04 == capabilities.Usage) || (0x05 == capabilities.Usage)));
    }

    // Get the base container id of a particular device instance
    GUID BaseContainerId(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        // Bail out when the device instance path is empty
        if (deviceInstancePath.empty()) return (GUID_NULL);

        DEVINST     devInst{};
        DEVPROPTYPE devPropType{};
        GUID        buffer{};
        ULONG       needed{ sizeof(buffer) };
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_ContainerId, &devPropType, reinterpret_cast<PBYTE>(&buffer), &needed, 0) }; (CR_SUCCESS != result))
        {
            // Bail out when the container id property isn't present
            if (CR_NO_SUCH_VALUE == result) return (GUID_NULL);
            THROW_CONFIGRET(result);
        }
        if (DEVPROP_TYPE_GUID != devPropType) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer);
    }

    // Get the parent of a particular device instance
    DeviceInstancePath DeviceInstancePathParent(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        DEVINST            devInst{};
        DEVPROPTYPE        devPropType{};
        DEVINST            devInstParent{};
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        ULONG              needed{ static_cast<ULONG>(buffer.size()) };
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_Parent(&devInstParent, devInst, 0) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInstParent, &DEVPKEY_Device_InstanceId, &devPropType, reinterpret_cast<PBYTE>(buffer.data()), &needed, 0) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (DEVPROP_TYPE_STRING != devPropType) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer.data());
    }

    // When a device has a base container id then get the device instance path of the base container id
    // Returns an empty string when the device doesn't have a base container id
    DeviceInstancePath BaseContainerDeviceInstancePath(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        auto const baseContainerId{ BaseContainerId(deviceInstancePath) };
        if ((GUID_NULL == baseContainerId) || (GUID_CONTAINER_ID_SYSTEM == baseContainerId)) return (std::wstring{});
        for (auto it{ deviceInstancePath };;)
        {
            if (auto const deviceInstancePathParent{ DeviceInstancePathParent(it) }; (baseContainerId == BaseContainerId(deviceInstancePathParent))) it = deviceInstancePathParent; else return (it);
        }
    }

    // Get the device class guid
    // For a base container device this is typically GUID_DEVCLASS_USB or GUID_DEVCLASS_HIDCLASS or GUID_DEVCLASS_XUSBCLASS 
    GUID DeviceClassGuid(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        // Bail out when the device instance path is empty
        if (deviceInstancePath.empty()) return (GUID_NULL);

        DEVINST     devInst{};
        DEVPROPTYPE devPropType{};
        GUID        buffer{};
        ULONG       needed{ sizeof(buffer) };
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_ClassGuid, &devPropType, reinterpret_cast<PBYTE>(&buffer), &needed, 0) }; (CR_SUCCESS != result)) return (GUID_NULL);
        if (DEVPROP_TYPE_GUID != devPropType) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer);
    }

    // Determine the number of child devices for a given base container device (the base control device itself is excluded from the count)
    // Returns zero when the device instance path is empty (typical for devices that aren't part of a base container)
    size_t BaseContainerDeviceCount(_In_ DeviceInstancePath const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        // Bail out when the device instance path is empty
        if (deviceInstancePath.empty()) return (0);

        // Prepare the iterator
        DEVINST devInst{};
        DEVINST devInstChild{};
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_Child(&devInstChild, devInst, 0) }; (CR_NO_SUCH_DEVNODE == result)) return (0); else if (CR_SUCCESS != result) THROW_CONFIGRET(result);

        // Iterate all child devices
        for (size_t count{ 1 };; count++)
        {
            if (auto const result{ ::CM_Get_Sibling(&devInstChild, devInstChild, 0) }; (CR_NO_SUCH_DEVNODE == result)) return (count); else if (CR_SUCCESS != result) THROW_CONFIGRET(result);
        }
    }

    // Collect descriptive information on the HID device and (when possible) interact with the device and query its model information
    HidHide::HidDeviceInformation HidModelInfo(_In_ DeviceInstancePath const& deviceInstancePath, _In_ std::filesystem::path const& symbolicLink)
    {
        TRACE_ALWAYS(L"");
        HidHide::HidDeviceInformation result{};
        result.gamingDevice                    = false;
        result.present                         = DevicePresent(deviceInstancePath);
        result.symbolicLink                    = symbolicLink;
        result.description                     = DeviceDescription(deviceInstancePath);
        result.deviceInstancePath              = deviceInstancePath;
        result.baseContainerDeviceInstancePath = BaseContainerDeviceInstancePath(deviceInstancePath);
        result.baseContainerClassGuid          = DeviceClassGuid(result.baseContainerDeviceInstancePath);
        result.baseContainerDeviceCount        = BaseContainerDeviceCount(result.baseContainerDeviceInstancePath);

        // Open a handle to communicate with the HID device
        auto const deviceObject{ CloseHandlePtr(::CreateFileW(symbolicLink.c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), &::CloseHandle) };
        if (INVALID_HANDLE_VALUE == deviceObject.get())
        {
            switch (::GetLastError())
            {
            case ERROR_ACCESS_DENIED:
                // The device is opened exclusively and in use hence we can't interact with it
            case ERROR_SHARING_VIOLATION:
                // The device is (most-likely) cloaked by Hid Hide itself while its client application isn't on the white-list
                result.usage = HidHide::StringTable(IDS_HID_ATTRIBUTE_DENIED);
                return (result);
            case ERROR_FILE_NOT_FOUND:
                // The device is currently not present hence we can't query its details
                result.usage = HidHide::StringTable(IDS_HID_ATTRIBUTE_ABSENT);
                return (result);
            default:
                THROW_WIN32_LAST_ERROR;
            }
        }

        // Prepare for device interactions
        PHIDP_PREPARSED_DATA preParsedData;
        if (FALSE == ::HidD_GetPreparsedData(deviceObject.get(), &preParsedData)) THROW_WIN32_LAST_ERROR;
        auto const freePreparsedDataPtr{ HidD_FreePreparsedDataPtr(preParsedData, &::HidD_FreePreparsedData) };

        // Get the usage page
        HIDP_CAPS capabilities;
        if (HIDP_STATUS_SUCCESS != ::HidP_GetCaps(preParsedData, &capabilities)) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Get the model information
        HIDD_ATTRIBUTES attributes;
        if (FALSE == ::HidD_GetAttributes(deviceObject.get(), &attributes)) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Get the model information
        std::vector<WCHAR> buffer(HidApiStringSizeInCharacters);
        result.product      = (FALSE == ::HidD_GetProductString     (deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.vendor       = (FALSE == ::HidD_GetManufacturerString(deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.serialNumber = (FALSE == ::HidD_GetSerialNumberString(deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.usage        = Usage(capabilities);
        result.gamingDevice = GamingDevice(attributes, capabilities);

        return (result);
    }

    // Merge model information into a single string
    void MergeModelInformation(_Inout_ std::vector<std::wstring>& model, _In_ std::wstring const& append)
    {
        TRACE_ALWAYS(L"");

        // Iterate all parts of the string offered
        for (auto const& part : HidHide::SplitStringAtWhitespaces(append))
        {
            // Add the part when its unique
            if (std::end(model) == std::find_if(std::begin(model), std::end(model), [part](std::wstring const& value) { return (0 == ::StrCmpLogicalW(value.c_str(), part.c_str())); }))
            {
                model.emplace_back(part);
            }
        }
    }
}

namespace HidHide
{
    _Use_decl_annotations_
    FriendlyNamesAndHidDeviceInformation HidDevices(bool gamingDevicesOnly)
    {
        TRACE_ALWAYS(L"");

        // Cluster/group the HID devices based on their base container device instance path
        BaseContainerDeviceInstancePathAndHidDeviceInformation baseContainerDeviceInstancePathAndHidDeviceInformation;
        IterateAllHidDevicesPresentOrNotPresent([&baseContainerDeviceInstancePathAndHidDeviceInformation](_In_ DeviceInstancePath const& deviceInstancePath, _In_ std::filesystem::path const& symbolicLink)
        {
            auto const hidDeviceInformation{ HidModelInfo(deviceInstancePath, symbolicLink) };
            baseContainerDeviceInstancePathAndHidDeviceInformation.emplace(std::make_pair(hidDeviceInformation.baseContainerDeviceInstancePath, hidDeviceInformation));
            return (true);
        });

        // Iterate all base container device instance paths and construct a friendly name
        FriendlyNamesAndHidDeviceInformation result;
        for (auto it{ std::begin(baseContainerDeviceInstancePathAndHidDeviceInformation) }; (std::end(baseContainerDeviceInstancePathAndHidDeviceInformation) != it); it++)
        {
            std::vector<std::wstring> model;

            // Keep track whether or not one of the devices in the container (if any) is a gaming device
            bool gamingDevice{ it->second.gamingDevice };
            FriendlyNamesAndHidDeviceInformation::mapped_type hidDevices;
            if (it->first.empty())
            {
                // When the device doesn't has a base container id then the friendly name will be based on just this one device
                MergeModelInformation(model, it->second.vendor);
                MergeModelInformation(model, it->second.product);
                hidDevices.emplace_back(it->second);
            }
            else
            {
                // When this device has a base container id then look for additional information in related entries
                for(auto const key{ it->first }; ((std::end(baseContainerDeviceInstancePathAndHidDeviceInformation) != it) && (it->first == key)); it++)
                {
                    gamingDevice = (gamingDevice || it->second.gamingDevice);
                    MergeModelInformation(model, it->second.vendor);
                    MergeModelInformation(model, it->second.product);
                    hidDevices.emplace_back(it->second);
                }

                // Move back to the last entry with the same base container id
                --it;
            }

            // Apply the gaming device only filter
            if ((!gamingDevicesOnly) || (gamingDevice))
            {
                FriendlyNamesAndHidDeviceInformation::key_type friendlyname;
                for (auto const& part : model)
                {
                    friendlyname = (friendlyname.empty() ? part : friendlyname + L" " + part);
                }

                // As a last resort, when everyting else fails, take the device description (this typically doesn't say anything but at least the box isn't empty)
                if (friendlyname.empty()) friendlyname = it->second.description;

                // Preserve the base container id and its children
                result.emplace(std::make_pair(friendlyname, hidDevices));
            }
        }

        // Process each unique base container instance path and replace 
        return (result);
    }
}
