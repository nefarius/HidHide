// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Config.h
#pragma once

// Conversion from HANDLE to PID
#define PROCESS_HANDLE_TO_PROCESS_ID(handle) (ULONG)((ULONG_PTR)handle & 0xFFFFFFFF)

EXTERN_C_START

// Register a PID and its load image
// Returns STATUS_PROCESS_IN_JOB (success) when the process id is already registered
// Returns STATUS_SUCCESS when the process id is registered, and the root of the tree may have changed
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS HidHideProcessIdRegister(_In_ WDFWAITLOCK wdfWaitLock, _In_ HANDLE processId, _In_ PUNICODE_STRING fullImageName);

// Unregister a PID
// When the PID isn't registered, the operation is ignored silently (STATUS_SUCCESS)
// When the PID is unregistered, the root of the tree may have changed
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS HidHideProcessIdUnregister(_In_ WDFWAITLOCK wdfWaitLock, _In_ HANDLE processId);

// Lookup the full image name associated with a registered process id and check it against the list of white-listed full image names provided
// cache-hit is TRUE when the result could be taken from the evaluation cache, or FALSE when the result had to be evaluated
// Returns STATUS_PROCESS_IN_JOB (Success) when the process id is known and the full image name is found in the string list provided
// Returns STATUS_PROCESS_NOT_IN_JOB (Success) when the process id is known and the full image name is not found in the string list provided
// Returns STATUS_SUCCESS when the process id isn't known
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideProcessIdCheckFullImageNameAgainstWhitelist(_In_ WDFWAITLOCK wdfWaitLock, _In_ HANDLE processId, _In_ WDFCOLLECTION wdfCollection, _Out_ BOOLEAN* cacheHit);

// Unregister all PIDs and return the new root (NULL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID HidHideProcessIdsCleanup(_In_ WDFWAITLOCK wdfWaitLock);

// Flush the currently cached evaluation results
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID HidHideProcessIdsFlushWhitelistEvaluationCache(_In_ WDFWAITLOCK wdfWaitLock);

// Run-time check on the btree algorithm
// Returns STATUS_SUCCESS when the algorithm passes the tests
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS HidHideVerifyInternalConsistency();

// Get a boolean driver property (DWORD)
// Returns STATUS_OBJECT_NAME_NOT_FOUND (Error) when the parameter is isn't found
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideDriverGetBooleanProperty(_In_ PCUNICODE_STRING valueName, _Out_ BOOLEAN* value);

// Set a boolean driver property (REG_DWORD)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideDriverSetBooleanProperty(_In_ PCUNICODE_STRING valueName, _In_ BOOLEAN value);

// Set a multi-string driver property (REG_MULTI_SZ)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideDriverSetMultiStringProperty(_In_ PCUNICODE_STRING valueName, _In_reads_(valueInCharacters) LPWSTR value, _In_ size_t valueInCharacters);

// Get a multi-string driver property and convert the string list into a string collection
// Returns STATUS_PROCESS_IN_JOB (Success) when the parameter is available and converted into a collection
// Returns STATUS_PROCESS_NOT_IN_JOB (Success) when the parameter isn't available (the collection returned is empty)
// On success the caller becomes responsible for calling WdfObjectDelete on the collection when the result is no longer needed
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideDriverCreateCollectionForMultiStringProperty(_In_ PCUNICODE_STRING valueName, _Out_ WDFCOLLECTION* value);

// Get a device instance path of a given device
// On success the caller becomes responsible for calling WdfObjectDelete on the object returned when it is no longer needed
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideDeviceInstancePath(_In_ WDFDEVICE wdfDevice, _Out_ WDFSTRING* deviceInstancePath);

// Get the multi-string from a string collection
// When the supplied buffer is NULL, the method returns STATUS_SUCCESS and indicates the buffer size needed for the multi-string (incl. terminator)
// When the supplied buffer isn't NULL, the whitelist will be copied into the buffer, providing the buffer is large enough for holding the result
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideCollectionToMultiString(_In_ WDFCOLLECTION wdfCollection, _Out_writes_to_opt_(bufferSizeInCharacters, *neededSizeInCharacters) LPWSTR buffer, _In_ size_t bufferSizeInCharacters, _Out_ size_t* neededSizeInCharacters);

EXTERN_C_END
