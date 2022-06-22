// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Config.h
#include "stdafx.h"
#include "Config.h"
#include "Logging.h"

// Assuming that the Lookup collection is constant and not subject to change, some performance can be gained by caching the lookup result
typedef enum
{
    EvaluationCacheEmpty = 0,
    EvaluationCacheFound,
    EvaluationCacheNotFound
} EvaluationCache;

// Binary-tree structure with data payload
typedef struct _PROCESSIDTREE
{
    ULONG                  pid;
    WCHAR                  fullImageName[NTSTRSAFE_UNICODE_STRING_MAX_CCH];
    UNICODE_STRING         fullImageNameUnicodeString;
    EvaluationCache        evaluationCache;
    struct _PROCESSIDTREE* left;
    struct _PROCESSIDTREE* right;
} PROCESSIDTREE, * PPROCESSIDTREE;

// The static b-tree used for registering a full load image with a process id
PPROCESSIDTREE s_ProcessIdToFullLoadImageNameMappingTree = NULL;

// Unique memory pool tag for the tree
#define CONFIG_TAG '1gaT'

// Insert the new node in the tree
// On success, the tree takes ownership of the node and its cleanup
// Return STATUS_ALREADY_INITIALIZED (Error) when the key wasn't unique
//
// When memory allocation/deallocation is time consuming, its may be more efficient
// to first try to lookup an exiting node and then attempt the insert
//
// When the change of failure is minimal, it may be more efficent not to do the double
// overhead of iterating the tree twice but instead do the memory allocation upfront
// and immediately attempt to insert it

_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS BstInsert(_Inout_ PPROCESSIDTREE* tree, _In_ PPROCESSIDTREE node);

// Delete a node from the tree (when present)
// Returns STATUS_PROCESS_IN_JOB (Success) when the key is found and deleted
// Returns STATUS_PROCESS_NOT_IN_JOB (Success) when the key isn't found
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS BstDelete(_Inout_ PPROCESSIDTREE* tree, _In_ ULONG pid);

// Create a new node for subsequent adding to the tree
// Returns the node on success or NULL on memory errors
_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS BstNewNode(_In_ ULONG pid, _In_ PUNICODE_STRING fullImageName, _Out_ PPROCESSIDTREE* node);

// Look for a pid in the tree
// Returns the node when found or NULL when not found
_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
PPROCESSIDTREE BstLookup(_In_opt_ PPROCESSIDTREE tree, _In_ ULONG pid);

// To the counter provider add the number of nodes found in the tree provided
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS BstAddCount(_In_opt_ PPROCESSIDTREE tree, _Inout_ rsize_t* count);

// Cleanup the whole tree
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID BstCleanup(_Inout_ PPROCESSIDTREE* tree);

// Flush the cached results
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID BstFlushEvaluationCache(_In_ PPROCESSIDTREE tree);

_Use_decl_annotations_
NTSTATUS BstInsert(PPROCESSIDTREE* tree, PPROCESSIDTREE node)
{
    TRACE_PERFORMANCE(L"");

    // Validate arguments
    if ((NULL == tree) || (NULL == node)) return (STATUS_INVALID_PARAMETER);

    // When the tree isn't yet containing any nodes then the new node becomes the root
    if (NULL == *tree)
    {
        *tree = node;
        return (STATUS_SUCCESS);
    }

    // Lower nodes go to the left
    if (node->pid < (*tree)->pid)
    {
        return (BstInsert(&(*tree)->left, node));
    }

    // Higher nodes go to the right
    if (node->pid > (*tree)->pid)
    {
        return (BstInsert(&(*tree)->right, node));
    }

    // Not left nor right then its a match and an error as the key needs to be unique
    return (STATUS_ALREADY_INITIALIZED);
}

_Use_decl_annotations_
NTSTATUS BstDelete(PPROCESSIDTREE* tree, ULONG pid)
{
    TRACE_PERFORMANCE(L"");

    PPROCESSIDTREE node;

    // Validate arguments
    if (NULL == tree) return (STATUS_INVALID_PARAMETER);

    // When the tree isn't yet containing any nodes then we are done quickly
    if (NULL == (*tree)) return (STATUS_PROCESS_NOT_IN_JOB);

    // All lower keys go to the left
    if (pid < (*tree)->pid) return (BstDelete(&(*tree)->left, pid));

    // All higher keys go to the right
    if (pid > (*tree)->pid) return (BstDelete(&(*tree)->right, pid));

    // Not left nor right then its a match

    // When the left tree is empty we can detach the node by altering its parent and replace
    // the reference to the node being deleted by its right tree (which could be empty)
    // New process ids are typically higher than the previous so better keep the right tree small
    if (NULL == (*tree)->left)
    {
        node = *tree;
        (*tree) = (*tree)->right;
        ExFreePoolWithTag(node, CONFIG_TAG);
        return (STATUS_PROCESS_IN_JOB);
    }

    // When the right tree is empty we can detach the node by altering its parent and replace
    // the reference to the node being deleted by its left tree (which could be empty)
    if (NULL == (*tree)->right)
    {
        node = *tree;
        (*tree) = (*tree)->left;
        ExFreePoolWithTag(node, CONFIG_TAG);
        return (STATUS_PROCESS_IN_JOB);
    }

    // The node to delete has a left and right tree so in its right branch find the lowest node,
    // attach the left tree to it, and move the root to the first node on the right
    for (node = (*tree)->right; (NULL != node->left); node = node->left);
    node->left = (*tree)->left;
    node = (*tree);
    (*tree) = (*tree)->right;
    ExFreePoolWithTag(node, CONFIG_TAG);
    return (STATUS_PROCESS_IN_JOB);
}

_Use_decl_annotations_
 NTSTATUS BstNewNode(ULONG pid, PUNICODE_STRING fullImageName, PPROCESSIDTREE* node)
{
    TRACE_PERFORMANCE(L"");

    PPROCESSIDTREE temp;
    NTSTATUS       ntstatus;

    // Bail out when memory allocation failed
    temp = ExAllocatePoolZero(NonPagedPool, sizeof(*temp), CONFIG_TAG);
    if (NULL == temp) LOG_AND_RETURN_NTSTATUS(L"ExAllocatePoolWithTag", STATUS_NO_MEMORY);

    // Ensure that the left and right pointers are NULL and the string is terminated
    RtlZeroMemory(temp, sizeof(*temp));
    temp->pid = pid;

    // Preserve the string content and append a string terminator
    ntstatus = RtlStringCchCopyUnicodeStringEx(&temp->fullImageName[0], _countof(temp->fullImageName), fullImageName, NULL, NULL, (STRSAFE_NO_TRUNCATION | STRSAFE_NULL_ON_FAILURE));
    if (!NT_SUCCESS(ntstatus))
    {
        ExFreePoolWithTag(temp, CONFIG_TAG);
        LOG_AND_RETURN_NTSTATUS(L"RtlStringCchCopyUnicodeStringEx", ntstatus);
    }
    // Construct the unicode string (not an actual copy but a reference to the existing string)
    ntstatus = RtlUnicodeStringInit(&temp->fullImageNameUnicodeString, &temp->fullImageName[0]);
    if (!NT_SUCCESS(ntstatus))
    {
        ExFreePoolWithTag(temp, CONFIG_TAG);
        LOG_AND_RETURN_NTSTATUS(L"RtlUnicodeStringInit", ntstatus);
    }

    *node = temp;
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
PPROCESSIDTREE BstLookup(PPROCESSIDTREE tree, ULONG pid)
{
    TRACE_PERFORMANCE(L"");

    if ((NULL != tree) && (tree->pid < pid)) return (BstLookup(tree->right, pid));
    if ((NULL != tree) && (tree->pid > pid)) return (BstLookup(tree->left, pid));
    return (tree);
}

_Use_decl_annotations_
NTSTATUS BstAddCount(PPROCESSIDTREE tree, rsize_t* count)
{
    TRACE_PERFORMANCE(L"");

    if (NULL != tree)
    {
        (*count)++;
        BstAddCount(tree->left, count);
        BstAddCount(tree->right, count);
    }
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID BstCleanup(PPROCESSIDTREE* tree)
{
    TRACE_PERFORMANCE(L"");

    if (NULL != *tree)
    {
        BstCleanup(&(*tree)->left);
        BstCleanup(&(*tree)->right);
        ExFreePoolWithTag(*tree, CONFIG_TAG);
        *tree = NULL;
    }
}

_Use_decl_annotations_
VOID BstFlushEvaluationCache(PPROCESSIDTREE tree)
{
    TRACE_PERFORMANCE(L"");

    if (NULL != tree)
    {
        BstFlushEvaluationCache(tree->left);
        BstFlushEvaluationCache(tree->right);
        tree->evaluationCache = EvaluationCacheEmpty;
    }
}

_Use_decl_annotations_
NTSTATUS HidHideProcessIdRegister(WDFWAITLOCK wdfWaitLock, HANDLE processId, PUNICODE_STRING fullImageName)
{
    TRACE_PERFORMANCE(L"");

    PPROCESSIDTREE node;
    NTSTATUS       ntstatus;

    WdfWaitLockAcquire(wdfWaitLock, NULL);

    // Do nothing when the pid is already registered
    node = BstLookup(s_ProcessIdToFullLoadImageNameMappingTree, PROCESS_HANDLE_TO_PROCESS_ID(processId));
    if (NULL != node)
    {
        WdfWaitLockRelease(wdfWaitLock);
        return (STATUS_PROCESS_IN_JOB);
    }

    // Create a new node for storage
    ntstatus = BstNewNode(PROCESS_HANDLE_TO_PROCESS_ID(processId), fullImageName, &node);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfWaitLockRelease(wdfWaitLock);
        return (ntstatus);
    }

    // Attempt to insert the node
    ntstatus = BstInsert(&s_ProcessIdToFullLoadImageNameMappingTree, node);
    if (!NT_SUCCESS(ntstatus))
    {
        ExFreePoolWithTag(node, CONFIG_TAG);
        WdfWaitLockRelease(wdfWaitLock);
        return (ntstatus);
    }

    WdfWaitLockRelease(wdfWaitLock);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS HidHideProcessIdUnregister(WDFWAITLOCK wdfWaitLock, HANDLE processId)
{
    TRACE_PERFORMANCE(L"");

    NTSTATUS ntstatus;

    WdfWaitLockAcquire(wdfWaitLock, NULL);
    ntstatus = BstDelete(&s_ProcessIdToFullLoadImageNameMappingTree, PROCESS_HANDLE_TO_PROCESS_ID(processId));
    WdfWaitLockRelease(wdfWaitLock);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"BstDelete", ntstatus);

    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS HidHideProcessIdCheckFullImageNameAgainstWhitelist(WDFWAITLOCK wdfWaitLock, HANDLE processId, WDFCOLLECTION wdfCollection, BOOLEAN* cacheHit)
{
    TRACE_PERFORMANCE(L"");

    PPROCESSIDTREE node;
    UNICODE_STRING fullImageName;

    // Validate arguments
    if (NULL == cacheHit) return (STATUS_INVALID_PARAMETER);

    WdfWaitLockAcquire(wdfWaitLock, NULL);

    // Is a full image name registered for this process id ?
    node = BstLookup(s_ProcessIdToFullLoadImageNameMappingTree, PROCESS_HANDLE_TO_PROCESS_ID(processId));
    (*cacheHit) = ((NULL != node) && (EvaluationCacheEmpty != node->evaluationCache));
    if (NULL == node)
    {
        // Its not known (this is acceptable behavior and not an error, hence return success)
        WdfWaitLockRelease(wdfWaitLock);
        return (STATUS_SUCCESS);
    }

    // When we are allowed to use the cache return not-found where applicable 
    if (EvaluationCacheFound == node->evaluationCache)
    {
        WdfWaitLockRelease(wdfWaitLock);
        return (STATUS_PROCESS_IN_JOB);
    }

    // When we are allowed to use the cache return not-found where applicable 
    if (EvaluationCacheNotFound == node->evaluationCache)
    {
        WdfWaitLockRelease(wdfWaitLock);
        return (STATUS_PROCESS_NOT_IN_JOB);
    }

    // The process is known so indicate for tracing purposes its full image name
    TRACE_ALWAYS(node->fullImageName);

    // Iterate all entries in the collection and look for a match while ignore case
    for (ULONG index = 0, size = WdfCollectionGetCount(wdfCollection); (index < size); index++)
    {
        WdfStringGetUnicodeString(WdfCollectionGetItem(wdfCollection, index), &fullImageName); // PASSIVE_LEVEL
        if (0 == RtlCompareUnicodeString(&fullImageName, &node->fullImageNameUnicodeString, TRUE))
        {
            // Found the process id
            node->evaluationCache = EvaluationCacheFound;
            WdfWaitLockRelease(wdfWaitLock);
            return (STATUS_PROCESS_IN_JOB);
        }
    }

    node->evaluationCache = EvaluationCacheNotFound;
    WdfWaitLockRelease(wdfWaitLock);

    // Process id was found but no matching full image name
    return (STATUS_PROCESS_NOT_IN_JOB);
}

_Use_decl_annotations_
VOID HidHideProcessIdsCleanup(WDFWAITLOCK wdfWaitLock)
{
    TRACE_ALWAYS(L"");

    WdfWaitLockAcquire(wdfWaitLock, NULL);
    BstCleanup(&s_ProcessIdToFullLoadImageNameMappingTree);
    WdfWaitLockRelease(wdfWaitLock);
}

_Use_decl_annotations_
VOID HidHideProcessIdsFlushWhitelistEvaluationCache(WDFWAITLOCK wdfWaitLock)
{
    TRACE_ALWAYS(L"");

    WdfWaitLockAcquire(wdfWaitLock, NULL);
    BstFlushEvaluationCache(s_ProcessIdToFullLoadImageNameMappingTree);
    WdfWaitLockRelease(wdfWaitLock);
}

ULONG s_testPattern[] = { 5, 11, 15, 10, 8, 9, 3, 4, 1, 2 };

_Use_decl_annotations_
NTSTATUS HidHideVerifyInternalConsistency()
{
    TRACE_ALWAYS(L"");

    PPROCESSIDTREE tree;
    PPROCESSIDTREE node;
    rsize_t        count;
    ULONG          index;
    NTSTATUS       ntstatus;

    DECLARE_UNICODE_STRING_SIZE(dummy, 5);

    tree = NULL;
    ntstatus = STATUS_SUCCESS;
    for (index = 0; (index < _countof(s_testPattern)); index++)
    {
        // The initial lookup should fail
        ntstatus = ((NULL == BstLookup(tree, s_testPattern[index])) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Initial lookup should fail");
            break;
        }

        // Create new btree node
        ntstatus = BstNewNode(s_testPattern[index], &dummy, &node);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Node creation should succeed");
            break;
        }

        // Add it to the tree
        ntstatus = BstInsert(&tree, node);
        if (!NT_SUCCESS(ntstatus))
        {
            ExFreePoolWithTag(node, CONFIG_TAG);
            TRACE_ALWAYS(L"Adding a unique node should succeed");
            break;
        }

        // The tree cannot be empty anymore
        ntstatus = ((NULL != tree) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After adding at least one node the tree can't be empty");
            break;
        }

        // The lookup after the insert should succeed
        ntstatus = ((NULL != BstLookup(tree, s_testPattern[index])) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Looking up a key after it is inserted should succeed");
            break;
        }

        // Add it again should fail
        ntstatus = ((STATUS_ALREADY_INITIALIZED == BstInsert(&tree, node)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Attempting to insert the same key again should fail");
            break;
        }
    }

    // The node count for left should be 4 (1,2,3,4)
    if (NT_SUCCESS(ntstatus))
    {
        count = 0;
        ntstatus = ((NULL != tree) && (NULL != tree->left) && (NT_SUCCESS(BstAddCount(tree->left, &count)) && (4 == count)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After an insert, the left branch should have the correct number of entries");
        }
    }

    // The node count for right should be 5 (8,9,10,11,15)
    if (NT_SUCCESS(ntstatus))
    {
        count = 0;
        ntstatus = ((NULL != tree) && (NULL != tree->right) && (NT_SUCCESS(BstAddCount(tree->right, &count)) && (5 == count)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After an insert, the right branch should have the correct number of entries");
        }
    }

    // Delete the tree root (5)
    if (NT_SUCCESS(ntstatus))
    {
        ntstatus = BstDelete(&tree, 5);
        ntstatus = ((STATUS_PROCESS_IN_JOB == ntstatus) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Deleting a known node should succeed");
        }
    }

    // Attempt to delete a non-existing node (5)
    if (NT_SUCCESS(ntstatus))
    {
        ntstatus = BstDelete(&tree, 5);
        ntstatus = ((STATUS_PROCESS_NOT_IN_JOB == ntstatus) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"Deleting an unknown node should succeed");
        }
    }

    // The left branch should now contain 7 nodes (1,2,3,4,8,9,10)
    if (NT_SUCCESS(ntstatus))
    {
        count = 0;
        ntstatus = ((NULL != tree) && (NULL != tree->left) && (NT_SUCCESS(BstAddCount(tree->left, &count)) && (7 == count)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After a delete, the left branch should have the correct number of entries");
        }
    }

    // The right branch should now contain 1 node (15)
    if (NT_SUCCESS(ntstatus))
    {
        count = 0;
        ntstatus = ((NULL != tree) && (NULL != tree->right) && (NT_SUCCESS(BstAddCount(tree->right, &count)) && (1 == count)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After a delete, the right branch should have the correct number of entries");
        }
    }

    // Release the allocated memory
    if (NT_SUCCESS(ntstatus))
    {
        BstCleanup(&tree);
        ntstatus = ((NULL == tree) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        if (!NT_SUCCESS(ntstatus))
        {
            TRACE_ALWAYS(L"After a clear the tree should be empty");
        }
    }

    // Report a failure only once
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"result ", ntstatus);

    return (ntstatus);
}

_Use_decl_annotations_
NTSTATUS HidHideDriverGetBooleanProperty(PCUNICODE_STRING valueName, BOOLEAN* value)
{
    TRACE_ALWAYS(L"");

    WDFKEY   wdfKey;
    ULONG    unsignedLong;
    NTSTATUS ntstatus;

    // Initialize return value
    *value = FALSE;

    // Get the filter drivers parameter key
    ntstatus = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_READ, WDF_NO_OBJECT_ATTRIBUTES, &wdfKey); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDriverOpenParametersRegistryKey", ntstatus);

    // Query property value
    ntstatus = WdfRegistryQueryULong(wdfKey, valueName, &unsignedLong);
    if (!NT_SUCCESS(ntstatus))
    {
        // Intercept error parameter-not-found and convert it into a success condition (we have a default)
        if (STATUS_OBJECT_NAME_NOT_FOUND == ntstatus) return (STATUS_PROCESS_NOT_IN_JOB);

        WdfRegistryClose(wdfKey);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryQueryULong", ntstatus);
    }

    // Convert unsigned long into a boolean
    *value = ((FALSE != unsignedLong) ? TRUE : FALSE);

    WdfRegistryClose(wdfKey);
    return (STATUS_PROCESS_IN_JOB);
}

_Use_decl_annotations_
NTSTATUS HidHideDriverSetBooleanProperty(PCUNICODE_STRING valueName, BOOLEAN value)
{
    TRACE_ALWAYS(L"");

    WDFKEY   wdfKey;
    ULONG    unsignedLong;
    NTSTATUS ntstatus;

    // Get the filter drivers parameter key
    ntstatus = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &wdfKey); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDriverOpenParametersRegistryKey", ntstatus);

    // Assign property value
    unsignedLong = (value ? TRUE : FALSE);
    ntstatus = WdfRegistryAssignValue(wdfKey, valueName, REG_DWORD , sizeof(unsignedLong), &unsignedLong);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRegistryClose(wdfKey);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryAssignValue", ntstatus);
    }

    WdfRegistryClose(wdfKey);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS HidHideDriverSetMultiStringProperty(PCUNICODE_STRING valueName, LPWSTR value, size_t valueInCharacters)
{
    TRACE_ALWAYS(L"");

    WDFKEY   wdfKey;
    NTSTATUS ntstatus;

    // Get the filter drivers parameter key
    ntstatus = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &wdfKey); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDriverOpenParametersRegistryKey", ntstatus);

    // Assign property value
    ntstatus = WdfRegistryAssignValue(wdfKey, valueName, REG_MULTI_SZ, (ULONG)(valueInCharacters * sizeof(WCHAR)), value);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRegistryClose(wdfKey);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryAssignValue", ntstatus);
    }

    WdfRegistryClose(wdfKey);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS HidHideDriverCreateCollectionForMultiStringProperty(PCUNICODE_STRING valueName, WDFCOLLECTION* value)
{
    TRACE_ALWAYS(L"");

    WDFKEY                wdfKey;
    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    ULONG                 sizeNeededInBytes;
    ULONG                 valueType;
    NTSTATUS              ntstatus;

    // Get the filter drivers parameter key
    ntstatus = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_READ, WDF_NO_OBJECT_ATTRIBUTES, &wdfKey); // PASSIVE_LEVEL
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDriverOpenParametersRegistryKey", ntstatus);

    // Create the collection
    ntstatus = WdfCollectionCreate(NULL, value);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfCollectionCreate", ntstatus);

    // We encountered some unexpected behavior in the WdfRegistryQueryMultiString method when the registry key is empty
    // To counter it, do the checking ourselfs, and bail out on empty strings to avoid a STATUS_OBJECT_TYPE_MISMATCH
    ntstatus = WdfRegistryQueryValue(wdfKey, valueName, 0, NULL, &sizeNeededInBytes, &valueType);
    if (STATUS_BUFFER_OVERFLOW == ntstatus) ntstatus = STATUS_SUCCESS;
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRegistryClose(wdfKey);

        // Convert the situation where we couldn't find the parameter or buffer-overflow (as we kept the buffer zero) into a success condition with empty set
        if (STATUS_OBJECT_NAME_NOT_FOUND == ntstatus) return (STATUS_PROCESS_NOT_IN_JOB);

        WdfObjectDelete(*value);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryQueryValue", ntstatus);
    }

    // Bail out on an incorrect type
    if (REG_MULTI_SZ != valueType)
    {
        WdfRegistryClose(wdfKey);
        WdfObjectDelete(*value);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryQueryValue", STATUS_OBJECT_TYPE_MISMATCH);
    }

    // Bail out when the string is empty (two null-terminators or less) as we have an empty set as result
    if (sizeNeededInBytes <= 4)
    {
        WdfRegistryClose(wdfKey);
        return (STATUS_PROCESS_NOT_IN_JOB);
    }

    // Query the property value
    WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);
    wdfObjectAttributes.ParentObject = *value;
    ntstatus = WdfRegistryQueryMultiString(wdfKey, valueName, &wdfObjectAttributes, *value);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfRegistryClose(wdfKey);
        WdfObjectDelete(*value);
        LOG_AND_RETURN_NTSTATUS(L"WdfRegistryQueryMultiString", ntstatus);
    }

    WdfRegistryClose(wdfKey);
    return (STATUS_PROCESS_IN_JOB);
}

_Use_decl_annotations_
NTSTATUS HidHideDeviceInstancePath(WDFDEVICE wdfDevice, WDFSTRING* deviceInstancePath)
{
    TRACE_ALWAYS(L"");

    WDF_DEVICE_PROPERTY_DATA wdfDevicePropertyData;
    WDF_OBJECT_ATTRIBUTES    wdfObjectAttributes;
    DEVPROPTYPE              devPropType;
    WDFMEMORY                wdfMemory;
    UNICODE_STRING           path;
    NTSTATUS                 ntstatus;

    // Initialize the result
    (*deviceInstancePath) = NULL;

    // Acquire the device instance path in a memory object
    WDF_DEVICE_PROPERTY_DATA_INIT(&wdfDevicePropertyData, &DEVPKEY_Device_InstanceId);
    WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);
    wdfObjectAttributes.ParentObject = wdfDevice;
    ntstatus = WdfDeviceAllocAndQueryPropertyEx(wdfDevice, &wdfDevicePropertyData, NonPagedPool, &wdfObjectAttributes, &wdfMemory, &devPropType);
    if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"WdfDeviceAllocAndQueryPropertyEx", ntstatus);
    if (DEVPROP_TYPE_STRING != devPropType)
    {
        WdfObjectDelete(wdfMemory);
        LOG_AND_RETURN_NTSTATUS(L"WdfDeviceAllocAndQueryPropertyEx", STATUS_INVALID_PARAMETER);
    }

    // Acquire a pointer to the content of the memory object
    ntstatus = RtlUnicodeStringInit(&path, WdfMemoryGetBuffer(wdfMemory, NULL));
    if (!NT_SUCCESS(ntstatus))
    {
        WdfObjectDelete(wdfMemory);
        LOG_AND_RETURN_NTSTATUS(L"RtlUnicodeStringInit", ntstatus);
    }

    // Copy the string
    ntstatus = WdfStringCreate(&path, &wdfObjectAttributes, deviceInstancePath);
    if (!NT_SUCCESS(ntstatus))
    {
        WdfObjectDelete(wdfMemory);
        LOG_AND_RETURN_NTSTATUS(L"WdfStringCreate", ntstatus);
    }

    // Release the memory object as it is no longer needed
    WdfObjectDelete(wdfMemory);
    return (STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS HidHideCollectionToMultiString(WDFCOLLECTION wdfCollection, LPWSTR buffer, size_t bufferSizeInCharacters, size_t* neededSizeInCharacters)
{
    TRACE_ALWAYS(L"");

    UNICODE_STRING  string;
    NTSTRSAFE_PWSTR destinationEnd;
    size_t          remaining;
    NTSTATUS        ntstatus;

    // Initialize output
    (*neededSizeInCharacters) = 0;

    // Determine the size needed for each string (incl terminator)
    for (ULONG index = 0, size = WdfCollectionGetCount(wdfCollection); (index < size); index++)
    {
        WdfStringGetUnicodeString(WdfCollectionGetItem(wdfCollection, index), &string); // PASSIVE_LEVEL
        (*neededSizeInCharacters) += string.Length;
        (*neededSizeInCharacters)++;
    }
    // Don't overlook the multi-string terminator
    (*neededSizeInCharacters)++;

    // Bail out when only the size is requested
    if (NULL == buffer) return (STATUS_SUCCESS);

    // Bail out when the buffer is too small
    if (bufferSizeInCharacters < (*neededSizeInCharacters)) LOG_AND_RETURN_NTSTATUS(L"CollectionToMultiString", STATUS_BUFFER_TOO_SMALL);

    // Copy the string content
    for (ULONG index = 0, size = WdfCollectionGetCount(wdfCollection); (index < size); index++)
    {
        WdfStringGetUnicodeString(WdfCollectionGetItem(wdfCollection, index), &string);

        // Add the string to it
        // Note that this line will fail with STATUS_INVALID_PARAMETER when the string buffer provided is too large
        ntstatus = RtlStringCchCopyUnicodeStringEx(buffer, bufferSizeInCharacters, &string, &destinationEnd, &remaining, STRSAFE_NO_TRUNCATION);
        if (!NT_SUCCESS(ntstatus)) LOG_AND_RETURN_NTSTATUS(L"RtlStringCchCopyUnicodeStringEx", ntstatus);

        // Move past the string terminator as we need to keep it
        buffer = destinationEnd + 1;
        bufferSizeInCharacters = remaining - 1;
    }

    // Append the multi-string terminator
    (*buffer) = 0;

    return (STATUS_SUCCESS);
}
