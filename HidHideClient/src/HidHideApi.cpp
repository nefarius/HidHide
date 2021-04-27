// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideApi.cpp
//
// An abstraction layer for the business logic. Relies on exception handling for error handling
// Ideally, this layer has no state behavior, and limits itself to status reporting only (supporting proper decision making)
//
#include "stdafx.h"
#include "HidHideApi.h"
#include "Logging.h"

typedef std::unique_ptr<std::remove_pointer<HDEVINFO>::type, decltype(&::SetupDiDestroyDeviceInfoList)> SetupDiDestroyDeviceInfoListPtr;
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::FindVolumeClose)> FindVolumeClosePtr;
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> CloseHandlePtr;
typedef std::unique_ptr<std::remove_pointer<PHIDP_PREPARSED_DATA>::type, decltype(&::HidD_FreePreparsedData)> HidD_FreePreparsedDataPtr;
typedef std::unique_ptr<std::remove_pointer<HGLOBAL>::type, decltype(&::GlobalUnlock)> GlobalUnlockPtr;
typedef std::stack<size_t> PortNumberStack;
typedef std::tuple<std::filesystem::path, GUID, CM_POWER_DATA> DeviceInstancePathAndClassGuidAndPowerState;

// The Microsoft API states that the maximum string size is device dependent. Limiting the string size to a particular device (like the USB limit)
// may fail for other devices. Its however also observed that the Microsoft API fails when the offered string size is rather large (like UNICODE_STRING_MAX_CHARS)
// Unfortunately, the actual max size the Microsoft API is handling doesn't seem to be specified explicitely hence we need to define it ourselfs.
// Experimenting with some calls revealed that the max buffer size currently supported is 0xFFF in bytes.
constexpr size_t HidApiStringSizeInCharacters{ 127 * sizeof(WCHAR) };

// The Hid Hide I/O control custom device type (range 32768 .. 65535)
constexpr auto IoControlDeviceType{ 32769u };

// The Hid Hide I/O control codes
constexpr auto IOCTL_GET_WHITELIST{ CTL_CODE(IoControlDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IOCTL_SET_WHITELIST{ CTL_CODE(IoControlDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IOCTL_GET_BLACKLIST{ CTL_CODE(IoControlDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IOCTL_SET_BLACKLIST{ CTL_CODE(IoControlDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IOCTL_GET_ACTIVE   { CTL_CODE(IoControlDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IOCTL_SET_ACTIVE   { CTL_CODE(IoControlDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA) };

namespace
{
    // Convert the error code into an error text
    std::wstring ErrorMessage(_In_ DWORD errorCode)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        ::FormatMessageW((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), nullptr, errorCode, LANG_USER_DEFAULT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
        return (buffer.data());
    }

    // Convert a guid into a string
    std::wstring GuidToString(_In_ GUID const& guid)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(39);
        if (0 == ::StringFromGUID2(guid, buffer.data(), static_cast<int>(buffer.size()))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer.data());
    }

    // Split the string at the white-spaces
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

    // Convert a list of strings into a multi-string
    std::vector<WCHAR> StringListToMultiString(_In_ std::vector<std::wstring> const& values)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> result;
        for (auto const& value : values)
        {
            auto const oldSize{ result.size() };
            auto const appendSize{ value.size() + 1 };
            result.resize(oldSize + appendSize);
            if (0 != ::wcsncpy_s(&result.at(oldSize), appendSize, value.c_str(), appendSize)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        }
        result.push_back(L'\0');
        return (result);
    }

    // Convert a multi-string into a list of strings
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

    // Get the DosDeviceName associated with a volume name
    std::filesystem::path DosDeviceNameForVolumeName(_In_ std::wstring const& volumeName)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        // Strip the leading '\\?\' and trailing '\' and isolate the Volume{} part in the volume name
        if (0 == ::QueryDosDeviceW(volumeName.substr(4, volumeName.size() - 5).c_str(), buffer.data(), static_cast<DWORD>(buffer.size()))) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    // Get the mount points associated with a volume
    std::set<std::filesystem::path> VolumeMountPoints(_In_ std::wstring const& volumeName)
    {
        TRACE_ALWAYS(L"");
        std::set<std::filesystem::path> result;

        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        DWORD needed{};
        if (FALSE == ::GetVolumePathNamesForVolumeNameW(volumeName.data(), buffer.data(), static_cast<DWORD>(buffer.size()), &needed))
        {
            if (ERROR_MORE_DATA != ::GetLastError()) THROW_WIN32_LAST_ERROR;
        }
        else
        {
            // Iterate all logical disks associated with this volume
            auto const list{ MultiStringToStringList(buffer) };
            for (auto it{ std::begin(list) }; (std::end(list) != it); it++) result.emplace(*it);
        }

        return (result);
    }

    // Find the volume mount point that could store the file specified
    // Returns an empty path when no suitable volume was found
    std::filesystem::path FindVolumeMountPointForFileStorage(_In_ std::filesystem::path const& logicalFileName)
    {
        // Prepare the volume iterator
        std::vector<WCHAR> volumeName(UNICODE_STRING_MAX_CHARS);
        auto const findVolumeClosePtr{ FindVolumeClosePtr(::FindFirstVolumeW(volumeName.data(), static_cast<DWORD>(volumeName.size())), &::FindVolumeClose) };
        if (INVALID_HANDLE_VALUE == findVolumeClosePtr.get()) THROW_WIN32_LAST_ERROR;

        // Keep iterating the volumes and find the most specific mount point that could store the file
        std::wstring mostSpecificVolumeMountPoint;
        while (true)
        {
            // Iterate all mount points for this volume
            for (auto const& it : VolumeMountPoints(volumeName.data()))
            {
                auto const volumeMountPoint{ it.native() };
                if (0 == volumeMountPoint.compare(0, std::wstring::npos, logicalFileName.native(), 0, volumeMountPoint.size()))
                {
                    if (volumeMountPoint.size() > mostSpecificVolumeMountPoint.size())
                    {
                        mostSpecificVolumeMountPoint = volumeMountPoint;
                    }
                }
            }

            // Move to the next volume
            if (FALSE == ::FindNextVolumeW(findVolumeClosePtr.get(), volumeName.data(), static_cast<DWORD>(volumeName.size())))
            {
                if (ERROR_NO_MORE_FILES != ::GetLastError()) THROW_WIN32_LAST_ERROR;

                // No more volumes to iterate
                break;
            }
        }

        return (mostSpecificVolumeMountPoint);
    }

    // Get the volume name associated with a volume mount point
    std::wstring VolumeNameForVolumeMountPoint(_In_ std::filesystem::path const& volumeMountPoint)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (FALSE == ::GetVolumeNameForVolumeMountPointW(volumeMountPoint.native().c_str(), buffer.data(), static_cast<DWORD>(buffer.size()))) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    // Get the Device Instance Paths of all devices of a given class (present or not present)
    // Note that performing a query for the Hid-Hide service controlled devices turned out not to work for VMWare environments hence query the whole list here and work from there
    std::vector<std::wstring> DeviceInstancePaths(_In_ GUID const& classGuid)
    {
        TRACE_ALWAYS(L"");
        ULONG needed{};
        if (auto const result{ ::CM_Get_Device_ID_List_SizeW(&needed, GuidToString(classGuid).c_str(), CM_GETIDLIST_FILTER_CLASS) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        std::vector<WCHAR> buffer(needed);
        if (auto const result{ ::CM_Get_Device_ID_ListW(GuidToString(classGuid).c_str(), buffer.data(), needed, CM_GETIDLIST_FILTER_CLASS) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        return (MultiStringToStringList(buffer));
    }

    // Get the device description
    std::wstring DeviceDescription(_In_ std::wstring const& deviceInstancePath)
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

    // Determine the number of child devices for a given base container device (the base control device itself is excluded from the count)
    // Returns zero when the device instance path is empty (typical for devices that aren't part of a base container)
    size_t BaseContainerDeviceCount(_In_ std::wstring const& deviceInstancePath)
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
        for(size_t count{ 1 };; count++)
        {
            if (auto const result{ ::CM_Get_Sibling(&devInstChild, devInstChild, 0) }; (CR_NO_SUCH_DEVNODE == result)) return (count); else if (CR_SUCCESS != result) THROW_CONFIGRET(result);
        }
    }

    // Get the base container id of a particular device instance
    GUID DeviceBaseContainerId(_In_ std::wstring const& deviceInstancePath)
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

    // Get the device class guid
    // For a base container device this is typically GUID_DEVCLASS_USB or GUID_DEVCLASS_HIDCLASS or GUID_DEVCLASS_XUSBCLASS 
    GUID DeviceClassGuid(_In_ std::wstring const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        // Bail out when the device instance path is empty
        if (deviceInstancePath.empty()) return (GUID_NULL);

        DEVINST     devInst{};
        DEVPROPTYPE devPropType{};
        GUID        buffer{};
        ULONG       needed{ sizeof(buffer) };
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_ClassGuid, &devPropType, reinterpret_cast<PBYTE>(&buffer), &needed, 0) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);
        if (DEVPROP_TYPE_GUID != devPropType) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (buffer);
    }

    // Is the device present or absent
    bool DevicePresent(_In_ std::wstring const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");
        DEVINST devInst{};
        if (auto const result{ ::CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(deviceInstancePath.c_str()), CM_LOCATE_DEVNODE_NORMAL) }; (CR_NO_SUCH_DEVNODE == result) || (CR_SUCCESS == result)) return (CR_SUCCESS == result); else THROW_CONFIGRET(result);
    }
     
    // Get the parent of a particular device instance
    std::wstring DeviceInstancePathParent(_In_ std::wstring const& deviceInstancePath)
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

    // Determine the Symbolic Link towards a particular device
    // Returns the Symbolic Link suited for CreateFile or an empty string when the device interface isn't supported
    std::filesystem::path SymbolicLink(_In_ GUID const& deviceInterfaceGuid, _In_ std::wstring const& deviceInstancePath)
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
            return (L"");
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

    // When a device has a base container id then get the device instance path of the base container id
    // Returns an empty string when the device doesn't have a base container id
    std::wstring DeviceInstancePathBaseContainerId(_In_ std::wstring const& deviceInstancePath)
    {
        TRACE_ALWAYS(L"");

        auto const baseContainerId{ DeviceBaseContainerId(deviceInstancePath) };
        if ((GUID_NULL == baseContainerId) || (GUID_CONTAINER_ID_SYSTEM == baseContainerId)) return (std::wstring{});
        for (auto it{ deviceInstancePath };;)
        {
            if (auto const deviceInstancePathParent{ DeviceInstancePathParent(it) }; (baseContainerId == DeviceBaseContainerId(deviceInstancePathParent))) it = deviceInstancePathParent; else return (it);
        }
    }

    // Determine from the model information and usage if this is a gaming device or not
    bool GamingDevice(_In_ HIDD_ATTRIBUTES const& attributes, _In_ HIDP_CAPS const& capabilities)
    {
        TRACE_ALWAYS(L"");

        // 0x28DE 0x1142 = Valve Corporation Steam Controller
        return (((attributes.VendorID == 0x28DE) && (attributes.ProductID == 0x1142)) || (0x05 == capabilities.UsagePage) || (0x01 == capabilities.UsagePage) && ((0x04 == capabilities.Usage) || (0x05 == capabilities.Usage)));
    }

    // Get the usage name from the usage page and usage id
    // Based on Revision 1.2 of standard (see also https://usb.org/sites/default/files/hut1_2.pdf)
    std::wstring UsageName(_In_ HIDP_CAPS const& capabilities)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);

        // For usage id 0x00 through 0xFF of page 1 we have a dedicated lookup table for the usage name at resource id offset 0x1100
        if ((0x01 == capabilities.UsagePage) && (0xFF >= capabilities.Usage) && (0 != ::LoadStringW(AfxGetApp()->m_hInstance, (0x1100 + capabilities.Usage), buffer.data(), static_cast<int>(buffer.size())))) return (buffer.data());

        // For usage id 0x00 through 0xFF of page 12 we have a dedicated lookup table for the usage name at resource id offset 0x1200
        if ((0x0C == capabilities.UsagePage) && (0xFF >= capabilities.Usage) && (0 != ::LoadStringW(AfxGetApp()->m_hInstance, (0x1200 + capabilities.Usage), buffer.data(), static_cast<int>(buffer.size())))) return (buffer.data());

        // For usage page 0x00 through 0xFF we have a dedicated lookup table for the page name at resource id offset 0x1000
        std::wostringstream os;
        os.exceptions(std::ostringstream::failbit | std::ostringstream::badbit);
        os << std::hex << std::uppercase << std::setfill(L'0') << std::setw(4);
        if ((0xFF >= capabilities.UsagePage) && (0 != ::LoadStringW(AfxGetApp()->m_hInstance, (0x1000 + capabilities.UsagePage), buffer.data(), static_cast<int>(buffer.size()))))
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

    // Interact with the HID device and query its product id, serial number, and manufacturer
    HidHide::HidDeviceInstancePathWithModelInfo HidModelInfo(_In_ std::wstring const& deviceInstancePath, _In_ std::filesystem::path const& symbolicLink)
    {
        TRACE_ALWAYS(L"");
        HidHide::HidDeviceInstancePathWithModelInfo result;
        result.gamingDevice                               = false;
        result.present                                    = DevicePresent(deviceInstancePath);
        result.description                                = DeviceDescription(deviceInstancePath);
        result.deviceInstancePath                         = deviceInstancePath;
        result.deviceInstancePathBaseContainer            = DeviceInstancePathBaseContainerId(deviceInstancePath);
        result.deviceInstancePathBaseContainerClassGuid   = DeviceClassGuid(result.deviceInstancePathBaseContainer);
        result.deviceInstancePathBaseContainerDeviceCount = BaseContainerDeviceCount(result.deviceInstancePathBaseContainer);

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
        if (HIDP_STATUS_SUCCESS != ::HidP_GetCaps(preParsedData, &result.capabilities)) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Get the model information
        if (FALSE == ::HidD_GetAttributes(deviceObject.get(), &result.attributes)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        
        // Get the model information
        std::vector<WCHAR> buffer(HidApiStringSizeInCharacters);
        result.product      = (FALSE == ::HidD_GetProductString     (deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.vendor       = (FALSE == ::HidD_GetManufacturerString(deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.serialNumber = (FALSE == ::HidD_GetSerialNumberString(deviceObject.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size()))) ? L"" : std::wstring(buffer.data());
        result.usage        = UsageName(result.capabilities);
        result.gamingDevice = GamingDevice(result.attributes, result.capabilities);

        return (result);
    }

    // Convert path set to string list
    std::vector<std::wstring> PathSetToStringList(_In_ std::set<std::filesystem::path> const& paths)
    {
        TRACE_ALWAYS(L"");
        std::vector<std::wstring> result;
        for (auto const& path : paths) result.emplace_back(path);
        return (result);
    }

    // Convert string list to path set
    std::set<std::filesystem::path> StringListToPathSet(_In_ std::vector<std::wstring> const& strings)
    {
        TRACE_ALWAYS(L"");
        std::set<std::filesystem::path> result;
        for (auto const& string : strings) result.emplace(string);
        return (result);
    }

    // Get a file handle to the device driver
    // The flag allowFileNotFound is applied when the device couldn't be found and controls whether or not an exception is thrown on failure
    CloseHandlePtr DeviceHandle(_In_ bool allowFileNotFound = false)
    {
        TRACE_ALWAYS(L"");
        auto handle{ CloseHandlePtr(::CreateFileW(HidHide::StringTable(IDS_CONTROL_SERVICE_PATH).c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), &::CloseHandle) };
        if ((INVALID_HANDLE_VALUE == handle.get()) && ((ERROR_FILE_NOT_FOUND != ::GetLastError()) || (!allowFileNotFound))) THROW_WIN32_LAST_ERROR;
        return (handle);
    }

    // Merge model information into a single string
    void MergeModelInformation(_Inout_ std::vector<std::wstring>& model, _In_ std::wstring const& append)
    {
        TRACE_ALWAYS(L"");

        // Iterate all parts of the string offered
        for (auto const& part : SplitStringAtWhitespaces(append))
        {
            // Add the part when its unique
            if (std::end(model) == std::find_if(std::begin(model), std::end(model), [part](std::wstring const& value) { return (0 == ::StrCmpLogicalW(value.c_str(), part.c_str())); }))
            {
                model.emplace_back(part);
            }
        }
    }

    // Create a flat list of human interface devices found and collect typical model information
    // PresentOnly allows for filtering-out any device that isn't yet present
    HidHide::HidDeviceInstancePathsWithModelInfo GetHidDeviceInstancePathsWithModelInfo()
    {
        TRACE_ALWAYS(L"");
        HidHide::HidDeviceInstancePathsWithModelInfo result;

        // Get the HID interface, as used by DirectInput
        GUID hidDeviceInterfaceGuid;
        ::HidD_GetHidGuid(&hidDeviceInterfaceGuid);

        // Get the Device Instance Paths of all HID devices
        // Note that the list includes base container devices that don't offer a HID interface
        for (auto const& deviceInstancePath : DeviceInstancePaths(GUID_DEVCLASS_HIDCLASS))
        {
            // Filter the list for devices with a HID interface
            if (auto const symbolicLink{ SymbolicLink(hidDeviceInterfaceGuid, deviceInstancePath) }; !symbolicLink.empty())
            {
                // Get the model information using the HID interface
                result.emplace_back(HidModelInfo(deviceInstancePath, symbolicLink));
            }
        }

        return (result);
    }
}

namespace HidHide
{
    _Use_decl_annotations_
    std::wstring StringTable(UINT stringTableResourceId)
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (0 == ::LoadStringW(AfxGetApp()->m_hInstance, stringTableResourceId, buffer.data(), static_cast<int>(buffer.size()))) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    std::filesystem::path ModuleFileName()
    {
        TRACE_ALWAYS(L"");
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (FALSE == ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()))) THROW_WIN32_LAST_ERROR;
        return (buffer.data());
    }

    _Use_decl_annotations_
    bool FileNameIsAnApplication(std::filesystem::path const& logicalFileName)
    {
        auto const extension{ logicalFileName.extension() };
        return ((L".exe" == extension) || (L".com" == extension) || (L".bin" == extension));
    }

    _Use_decl_annotations_
    std::set<std::filesystem::path> DragTargetFileNames(COleDataObject* pDataObject)
    {
        std::set<std::filesystem::path> result;

        if (auto const hGlobal{ (nullptr == pDataObject) ? nullptr : pDataObject->GetGlobalData(CF_HDROP) }; (nullptr != hGlobal))
        {
            if (auto const hDrop{ GlobalUnlockPtr(::GlobalLock(hGlobal), &::GlobalUnlock) }; (nullptr != hDrop.get()))
            {
                for (UINT index{}, size{ ::DragQueryFileW(static_cast<HDROP>(hDrop.get()), 0xFFFFFFFF, nullptr, 0) }; (index < size); index++)
                {
                    std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
                    if (0 != ::DragQueryFileW(static_cast<HDROP>(hDrop.get()), index, buffer.data(), static_cast<UINT>(buffer.size())))
                    {
                        result.emplace(buffer.data());
                    }
                }
            }
        }

        return (result);
    }

    DescriptionToHidDeviceInstancePathsWithModelInfo GetDescriptionToHidDeviceInstancePathsWithModelInfo()
    {
        TRACE_ALWAYS(L"");
        DescriptionToHidDeviceInstancePathsWithModelInfo result;

        // Get all human interface devices and process each entry
        auto const hidDeviceInstancePathsWithModelInfo{ GetHidDeviceInstancePathsWithModelInfo()};
        for (auto it{ std::begin(hidDeviceInstancePathsWithModelInfo) }; (std::end(hidDeviceInstancePathsWithModelInfo) != it); it++)
        {
            auto const hasBaseContainerId{ (!it->deviceInstancePathBaseContainer.empty()) };

            // Skip already processed entries (checked by looking for the Device Instance Path of the Base Container in earlier entries)
            if (hasBaseContainerId)
            {
                if (it != std::find_if(std::begin(hidDeviceInstancePathsWithModelInfo), it, [it](HidDeviceInstancePathWithModelInfo const& value) { return (value.deviceInstancePathBaseContainer == it->deviceInstancePathBaseContainer); }))
                {
                    continue;
                }
            }

            // Merge manufacturer and product info
            std::vector<std::wstring> model;
            MergeModelInformation(model, it->vendor);
            MergeModelInformation(model, it->product);

            // When this device has a base container id then look for additional information in related entries
            HidDeviceInstancePathsWithModelInfo children;
            children.emplace_back(*it);
            if (hasBaseContainerId)
            {
                for (auto ahead{ it + 1 }; (std::end(hidDeviceInstancePathsWithModelInfo) != ahead); ahead++)
                {
                    if (ahead->deviceInstancePathBaseContainer == it->deviceInstancePathBaseContainer)
                    {
                        children.emplace_back(*ahead);
                        MergeModelInformation(model, ahead->vendor);
                        MergeModelInformation(model, ahead->product);
                    }
                }
            }

            // Combine the information into a top-level description
            std::wstring description;
            for (auto const& part : model)
            {
                description = (description.empty() ? part : description + L" " + part);
            }

            // As a last resort, when everyting else fails, take the device description (this typically doesn't say anything but at least the box isn't empty)
            if (description.empty()) description = DeviceDescription(it->deviceInstancePath);

            // Preserve the base container id and its children
            result.emplace_back(std::make_pair(description, children));
        }

        return (result);
    }

    _Use_decl_annotations_
    FullImageName FileNameToFullImageName(std::filesystem::path const& logicalFileName)
    {
        TRACE_ALWAYS(L"");
        auto const volumeMountPoint{ FindVolumeMountPointForFileStorage(logicalFileName) };
        if (volumeMountPoint.empty()) return (L"");
        auto const dosDeviceNameForVolumeName{ DosDeviceNameForVolumeName(VolumeNameForVolumeMountPoint(volumeMountPoint)) };
        auto const fileNameWithoutMountPoint{ std::filesystem::path(logicalFileName.native().substr(volumeMountPoint.native().size())) };
        return (dosDeviceNameForVolumeName / fileNameWithoutMountPoint);
    }

    _Use_decl_annotations_
    void AddApplicationToWhitelist(std::filesystem::path const& logicalFileName)
    {
        TRACE_ALWAYS(L"");
        if (auto whitelist{ GetWhitelist() }; whitelist.emplace(FileNameToFullImageName(logicalFileName)).second)
        {
            // The program wasn't yet on the list so update the white list
            SetWhitelist(whitelist);
        }
    }

    bool Present()
    {
        TRACE_ALWAYS(L"");
        return (INVALID_HANDLE_VALUE != DeviceHandle(true).get());
    }

    HidDeviceInstancePaths GetBlacklist()
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_GET_BLACKLIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_GET_BLACKLIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (MultiStringToStringList(buffer));
    }

    _Use_decl_annotations_
    void SetBlacklist(HidDeviceInstancePaths const& hidDeviceInstancePaths)
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        auto buffer{ StringListToMultiString(hidDeviceInstancePaths) };
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_SET_BLACKLIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    FullImageNames GetWhitelist()
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_GET_WHITELIST, nullptr, 0, nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        auto buffer{ std::vector<WCHAR>(needed) };
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_GET_WHITELIST, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        return (StringListToPathSet(MultiStringToStringList(buffer)));
    }

    _Use_decl_annotations_
    void SetWhitelist(FullImageNames const& applicationsOnWhitelist)
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        auto buffer{ StringListToMultiString(PathSetToStringList(applicationsOnWhitelist)) };
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_SET_WHITELIST, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }

    bool GetActive()
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        auto buffer{ std::vector<BOOLEAN>(1) };
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_GET_ACTIVE, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), &needed, nullptr)) THROW_WIN32_LAST_ERROR;
        if (sizeof(BOOLEAN) != needed) THROW_WIN32(ERROR_INVALID_PARAMETER);
        return (FALSE != buffer.at(0));
    }

    _Use_decl_annotations_
    void SetActive(bool active)
    {
        TRACE_ALWAYS(L"");
        DWORD needed;
        auto buffer{ std::vector<BOOLEAN>(1) };
        buffer.at(0) = (active ? TRUE : FALSE);
        if (FALSE == ::DeviceIoControl(DeviceHandle().get(), IOCTL_SET_ACTIVE, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), nullptr, 0, &needed, nullptr)) THROW_WIN32_LAST_ERROR;
    }
}
