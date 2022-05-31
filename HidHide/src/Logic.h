// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Logic.h
#pragma once

#define DEVICE_HARDWARE_ID                                L"root\\HidHide"
#define CONTROL_DEVICE_NT_DEVICE_NAME                     L"\\Device\\HidHide"
#define CONTROL_DEVICE_DOS_DEVICE_NAME                    L"\\DosDevices\\HidHide"
#define DRIVER_PROPERTY_WHITELISTED_FULL_IMAGE_NAMES      L"WhitelistedFullImageNames"      // HKLM\SYSTEM\CurrentControlSet\Services\HidHide\Parameters\WhitelistedFullImageNames (REG_MULTI_Z)
#define DRIVER_PROPERTY_BLACKLISTED_DEVICE_INSTANCE_PATHS L"BlacklistedDeviceInstancePaths" // HKLM\SYSTEM\CurrentControlSet\Services\HidHide\Parameters\BlacklistedDeviceInstancePaths (REG_MULTI_Z)
#define DRIVER_PROPERTY_ACTIVE                            L"Active"                         // HKLM\SYSTEM\CurrentControlSet\Services\HidHide\Parameters\Active (DWORD)
#define DRIVER_PROPERTY_WHITELISTED_INVERSE               L"WhitelistedInverse"             // HKLM\SYSTEM\CurrentControlSet\Services\HidHide\Parameters\WhitelistedInverse (DWORD)

// The Hid Hide I/O control custom device type (range 32768 .. 65535)
#define IoControlDeviceType 32769

// The Hid Hide I/O control codes
#define IOCTL_GET_WHITELIST CTL_CODE(IoControlDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WHITELIST CTL_CODE(IoControlDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_BLACKLIST CTL_CODE(IoControlDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_BLACKLIST CTL_CODE(IoControlDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_ACTIVE    CTL_CODE(IoControlDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_ACTIVE    CTL_CODE(IoControlDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_WLINVERSE CTL_CODE(IoControlDeviceType, 2054, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WLINVERSE CTL_CODE(IoControlDeviceType, 2055, METHOD_BUFFERED, FILE_READ_DATA)

// {0C320FF7-BD9B-42B6-BDAF-49FEB9C91649}
DEFINE_GUID(HidHideInterfaceGuid, 0xc320ff7, 0xbd9b, 0x42b6, 0xbd, 0xaf, 0x49, 0xfe, 0xb9, 0xc9, 0x16, 0x49);

// The administration maintained per device (0 .. *)
typedef struct _DEVICE_CONTEXT
{
    // The unique device instance path of this device, suitable as input for CreateFile
    WDFSTRING deviceInstancePath;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

// The administration shared by all devices (0 .. 1)
typedef struct _CONTROL_DEVICE_CONTEXT
{
    // Collection of string objects containing the full image names of applications that will be granted access to blacklisted human interface devices
    WDFCOLLECTION whitelistedFullImageNames;

    // Collection of string objects containing the device instance paths of the human interface devices (HID) that are subject to access control
    WDFCOLLECTION blacklistedDeviceInstancePaths;

    // The device active (enabled) state
    BOOLEAN active;

    // The whitelisted inverse (enabled) state
    BOOLEAN whitelistedInverse;

    // During a shutdown we may only delete the control device object after the last device is removed so keep track of the number of devices and shutdown state
    BOOLEAN shutdownPending;
    INT32 numberOfDevicesCreated;
} CONTROL_DEVICE_CONTEXT, *PCONTROL_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTROL_DEVICE_CONTEXT, ControlDeviceGetContext)

EXTERN_C_START

// Notifies the driver that the system is about to lose its power
EVT_WDF_DEVICE_SHUTDOWN_NOTIFICATION OnSystemShutdown;

// Notification handler called on the system-wide creation and deletion of (any) processes
CREATE_PROCESS_NOTIFY_ROUTINE OnSystemProcessChange;

// Notification handler called whenever (any) image is loaded (or mapped into memory)
LOAD_IMAGE_NOTIFY_ROUTINE OnSystemLoadImage;

// Notification handler called when either the framework or a driver attempts to delete the device object
EVT_WDF_DEVICE_CONTEXT_CLEANUP OnDeviceContextCleanup;

// Notification handler called when a user application or another driver opens the device to perform an I/O operation, such as reading or writing a file
EVT_WDF_DEVICE_FILE_CREATE OnDeviceFileCreate;

// Notification handler called when another driver completes a specified I/O request
EVT_WDF_REQUEST_COMPLETION_ROUTINE OnDeviceFileCreateRequestCompletion;

// Notification handler called when the last handle to the specified file object has been closed
EVT_WDF_FILE_CLEANUP OnDeviceFileCleanup;

// Notification handler called when either the framework or a driver attempts to delete the device object
EVT_WDF_DEVICE_CONTEXT_CLEANUP OnControlDeviceContextCleanup;

// Notification handler called when a user application or another driver opens the device to perform an I/O operation, such as reading or writing a file
EVT_WDF_DEVICE_FILE_CREATE OnControlDeviceFileCreate;

// Notification handler called when the last handle to the specified file object has been closed
EVT_WDF_FILE_CLEANUP OnControlDeviceFileCleanup;

// Hook called after having the driver created
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS OnDriverCreate(_In_ WDFDRIVER wdfDriver);

// Hook called after having a device created
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS OnDeviceCreate(_In_ WDFDEVICE wdfDevice);

// Hook called after having the control device created
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS OnControlDeviceCreate(_In_ WDFDEVICE wdfControlDevice);

// Hook called when a device I/O request for the control device is incoming
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoDeviceControl(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle GetWhitelist I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoGetWhitelist(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle SetWhitelist I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoSetWhitelist(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle GetBlacklist I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoGetBlacklist(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle SetBlacklist I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoSetBlacklist(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle GetActive I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoGetActive(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle SetActive I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoSetActive(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle GetInverse I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoGetInverse(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Handle SetInverse I/O request from client
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS OnControlDeviceIoSetInverse(_In_ WDFDEVICE wdfDevice, _In_ WDFQUEUE wdfQueue, _In_ WDFREQUEST wdfRequest, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode);

// Is the process id on the whitelist?
// On a match, the cache-hit indicates if its the first time or not
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN Whitelisted(_In_ HANDLE processId, _Out_ BOOLEAN* cacheHit);

// Is this device instance on the blacklist?
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN Blacklisted(_In_ PUNICODE_STRING deviceInstancePath);

// Get the whitelist in a multi-string format
// When the supplied buffer is NULL, the method returns STATUS_SUCCESS and indicates the buffer size needed for the multi-string (incl. terminator)
// When the supplied buffer isn't NULL, the list will be copied into the buffer, providing the buffer is large enough for holding the result
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS GetWhitelist(_Out_writes_to_opt_(bufferSizeInCharacters, *neededSizeInCharacters) LPWSTR buffer, _In_ size_t bufferSizeInCharacters, _Out_ size_t* neededSizeInCharacters);

// Set the whitelist in a multi-string format
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS SetWhitelist(_In_reads_(bufferSizeInCharacters) LPWSTR buffer, _In_ size_t bufferSizeInCharacters);

// Get the blacklist in a multi-string format
// When the supplied buffer is NULL, the method returns STATUS_SUCCESS and indicates the buffer size needed for the multi-string (incl. terminator)
// When the supplied buffer isn't NULL, the list will be copied into the buffer, providing the buffer is large enough for holding the result
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS GetBlacklist(_Out_writes_to_opt_(bufferSizeInCharacters, *neededSizeInCharacters) LPWSTR buffer, _In_ size_t bufferSizeInCharacters, _Out_ size_t* neededSizeInCharacters);

// Set the blacklist in a multi-string format
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS SetBlacklist(_In_reads_(bufferSizeInCharacters) LPWSTR buffer, _In_ size_t bufferSizeInCharacters);

// Get the active state (enable/disable service)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN GetActive();

// Set the active state (enable/disable service)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS SetActive(_In_ BOOLEAN active);

// Get the inverse state (enable/disable whitelist inverse)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN GetInverse();

// Set the inverse state (enable/disable whitelist inverse)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS SetInverse(_In_ BOOLEAN inverse);

// Update data for control device deletion and delete the control device when needed
// A positive number indicates that we have one or more new filter devices created
// A negative number indicates the number of filter devices destroyed
// A value of zero indicates that we entered the shutdown state
// In the shutdown state, the control device is deleted when there are no filter devices left
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID UpdateDataForControlDeviceDeletionAndDeleteControlDeviceWhenNeeded(_In_ INT32 increment);

EXTERN_C_END
