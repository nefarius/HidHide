// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Config.h
#pragma once

namespace HidHide
{
    // Interface for forwarding drop target events
    class IDropTarget
    {
    public:

        // Called when the cursor first enters the window
        virtual DROPEFFECT OnDragEnter(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dwKeyState);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }

        // Called repeatedly when the cursor is dragged over the window
        virtual DROPEFFECT OnDragOver(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dwKeyState);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }

        // Called when data is dropped into the window, initial handler
        virtual DROPEFFECT OnDropEx(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DROPEFFECT dropDefault, _In_ DROPEFFECT dropList, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dropDefault);
            UNREFERENCED_PARAMETER(dropList);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }
    };

    struct HidDeviceInstancePathWithModelInfo
    {
        HIDD_ATTRIBUTES attributes{};
        HIDP_CAPS       capabilities{};
        bool            present{};
        bool            gamingDevice{};
        std::wstring    vendor{};
        std::wstring    product{};
        std::wstring    serialNumber{};
        std::wstring    usage{};
        std::wstring    description{};
        std::wstring    deviceInstancePath{};
        std::wstring    deviceInstancePathBaseContainer{};
        GUID            deviceInstancePathBaseContainerClassGuid{};
        size_t          deviceInstancePathBaseContainerDeviceCount{};
    };

    typedef std::vector<HidDeviceInstancePathWithModelInfo> HidDeviceInstancePathsWithModelInfo;
    typedef std::vector<std::wstring> HidDeviceInstancePaths;
    typedef std::filesystem::path FullImageName;
    typedef std::set<FullImageName> FullImageNames;
    typedef std::vector<std::pair<std::wstring, HidHide::HidDeviceInstancePathsWithModelInfo>> DescriptionToHidDeviceInstancePathsWithModelInfo;

    // Lookup a value from the string table resource based on the resource id provided
    std::wstring StringTable(_In_ UINT stringTableResourceId);

    // Get the logical file name of this application
    std::filesystem::path ModuleFileName();

    // Determine if the file is an application (.exe, .com, or .bin)
    bool FileNameIsAnApplication(_In_ std::filesystem::path const& logicalFileName);

    // Determine if the key state matches that of a copy operation (left mouse button + with or without control)
    constexpr bool DragTargetCopyOperation(_In_ DWORD keyState) noexcept
    {
        return ((0 != (MK_LBUTTON & keyState)) && (0 == (MK_MBUTTON & keyState)) && (0 == (MK_RBUTTON & keyState)) && (0 == (MK_SHIFT & keyState)) && (0 == (MK_ALT & keyState)));
    }

    // Get the logical file name that is being dragged
    // Returns an empty set when the data dragged isn't a list of files
    std::set<std::filesystem::path> DragTargetFileNames(_In_ COleDataObject* pDataObject);

    // Create a hierarchical tree of human interface devices starting at the base container id level
    DescriptionToHidDeviceInstancePathsWithModelInfo GetDescriptionToHidDeviceInstancePathsWithModelInfo();

    // Determine the full image name for storage of the file specified while considering mounted folder structures
    // Return an empty path when the specified file name can't be stored on any of the volumes present
    FullImageName FileNameToFullImageName(_In_ std::filesystem::path const& logicalFileName);

    // Add an application to the white list providing it isn't on the list already
    // Performance wise this method is expensive as it utilizes GetWhitelist
    // For larger whitelist manipulations be sure to use SetWhitelist instead
    void AddApplicationToWhitelist(_In_ std::filesystem::path const& logicalFileName);

    // Get the control device state
    // Returns ERROR_SUCCESS when available for use
    // Returns FILE_NOT_FOUND when the device is disabled (assuming it is installed)
    // Returns ACCESS_DENIED when in use (assuming it is not an ACL issue)
    DWORD DeviceStatus();

    // Get the device Instance Paths of the Human Interface Devices that are on the black-list (may reference not present devices)
    HidDeviceInstancePaths GetBlacklist();

    // Set the device Instance Paths of the Human Interface Devices that are on the black-list
    void SetBlacklist(_In_ HidDeviceInstancePaths const& hidDeviceInstancePaths);

    // Get the applications on the white-list
    FullImageNames GetWhitelist();

    // Set the applications on the white-list
    void SetWhitelist(_In_ FullImageNames const& fullImageNames);

    // Get the current enabled state; returns true when the device is active in hiding devices on the black-list
    bool GetActive();

    // Set the current enabled state
    void SetActive(_In_ bool active);
}
