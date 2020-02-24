/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform.c

Abstract:

    This file contains the lower-edge of DMF. The code is this file is modified as
    needed for every custom (non-Windows) platform where DMF is compiled.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework
    Win32 Application

--*/

#include "DmfIncludeInternal.h"

#include "DmfPlatform.tmh"

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

// PLATFORM_TEMPLATE: Or with definitions of platforms that need platform support.
//
#if defined(DMF_WIN32_MODE) || defined(DMF_XXX_MODE)

///////////////////////////////////////////////////////////////////////////////////
// WDFOBJECT
///////////////////////////////////////////////////////////////////////////////////
//

// All objects can have a custom context assigned. This helper function is called
// by all object creation functions.
//
NTSTATUS
CustomContextAllocate(WDFOBJECT Object,
                      WDF_OBJECT_ATTRIBUTES* Attributes
    )
{
    NTSTATUS ntStatus;

    if (Attributes != WDF_NO_OBJECT_ATTRIBUTES && 
        Attributes->ContextTypeInfo != NULL &&
        Attributes->ContextTypeInfo->ContextSize > 0)
    {
        ntStatus = WdfObjectAllocateContext(Object,
                                            Attributes,
                                            NULL);
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

// TODO: Use ContextSizeOverride.
//
NTSTATUS
DmfPlatform_WdfObjectAllocateContext(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFOBJECT Handle,
    _In_
    PWDF_OBJECT_ATTRIBUTES ContextAttributes,
    _Outptr_opt_
    PVOID* Context
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_CONTEXT* platformContext;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Handle;

    platformContext = (DMF_PLATFORM_CONTEXT*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_CONTEXT));
    if (platformContext == NULL)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    platformContext->ContextData = DMF_Platform_Allocate(ContextAttributes->ContextTypeInfo->ContextSize);
    if (platformContext->ContextData == NULL)
    {
        DMF_Platform_Free(platformContext);
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    memcpy(&platformContext->ContextTypeInfo,
           ContextAttributes->ContextTypeInfo,
           sizeof(WDF_OBJECT_CONTEXT_TYPE_INFO));

    InitializeListHead(&platformContext->ListEntry);

    DMF_Platform_CriticalSectionEnter(&platformObject->ContextListLock);
    InsertTailList(&platformObject->ContextList,
                   &platformContext->ListEntry);
    DMF_Platform_CriticalSectionLeave(&platformObject->ContextListLock);

    if (Context != NULL)
    {
        *Context = platformContext->ContextData;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

PVOID
DmfPlatform_WdfObjectGetTypedContextWorker(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFOBJECT Handle,
    _In_
    PCWDF_OBJECT_CONTEXT_TYPE_INFO TypeInfo
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_CONTEXT* platformContext;
    LIST_ENTRY* listEntry;
    VOID* returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    returnValue = NULL;
    platformObject = (DMF_PLATFORM_OBJECT*)Handle;
    DMF_Platform_CriticalSectionEnter(&platformObject->ContextListLock);
    listEntry = platformObject->ContextList.Flink;
    while (listEntry != &platformObject->ContextList)
    {
        platformContext = CONTAINING_RECORD(listEntry,
                                            DMF_PLATFORM_CONTEXT,
                                            ListEntry);
        if (TypeInfo->ContextName == platformContext->ContextTypeInfo.ContextName)
        {
            returnValue = platformContext->ContextData;
            break;
        }
        listEntry = listEntry->Flink;
    }
    DMF_Platform_CriticalSectionLeave(&platformObject->ContextListLock);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfObjectDelete(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFOBJECT Object
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* childObject;
    LIST_ENTRY* listEntry;
    LONG newReferenceCount;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Object;

    // Decrement reference count.
    //
    newReferenceCount = InterlockedDecrement(&platformObject->ReferenceCount);

    // Always call clean up callback for each decrement.
    //
    if (platformObject->ObjectAttributes.EvtCleanupCallback != NULL)
    {
        platformObject->ObjectAttributes.EvtCleanupCallback(platformObject);
    }

    // Actually destroy object when reference count is zero.
    //
    if (newReferenceCount == 0)
    {
        // Create a list of all the objects to be deleted without the lock held.
        //
        DMF_Platform_CriticalSectionEnter(&platformObject->ChildListLock);
        listEntry = platformObject->ChildList.Flink;
        while (listEntry != &platformObject->ChildList)
        {
            DmfAssert(platformObject->NumberOfChildren >= 0);
            childObject = CONTAINING_RECORD(listEntry,
                                            DMF_PLATFORM_OBJECT,
                                            ChildListEntry);
            // NOTE: Number of children is decremented when object is removed from
            //       list. It is not removed from list now.
            //
            WdfObjectDelete(childObject);
            // The first object is the next object in the list (if present).
            //
            listEntry = platformObject->ChildList.Flink;
        }
        DMF_Platform_CriticalSectionLeave(&platformObject->ChildListLock);

        // Call the destroy callback.
        //
        if (platformObject->ObjectAttributes.EvtDestroyCallback != NULL)
        {
            platformObject->ObjectAttributes.EvtDestroyCallback(platformObject);
        }

        // Remove this object from its parent.
        //
        if (platformObject->ObjectAttributes.ParentObject != NULL)
        {
            DMF_PLATFORM_OBJECT* parentPlatformObject = (DMF_PLATFORM_OBJECT*)platformObject->ObjectAttributes.ParentObject;
            DMF_Platform_CriticalSectionEnter(&parentPlatformObject->ChildListLock);
            DmfAssert(parentPlatformObject->NumberOfChildren >= 0);
            RemoveEntryList(&platformObject->ChildListEntry);
            parentPlatformObject->NumberOfChildren--;
            DmfAssert(parentPlatformObject->NumberOfChildren >= 0);
            DMF_Platform_CriticalSectionLeave(&parentPlatformObject->ChildListLock);
        }
        // Free the memory used by this object.
        //
        if (platformObject->ObjectDelete != NULL)
        {
            // Free the object specific data.
            //
            platformObject->ObjectDelete(platformObject);
        }
        // Free the container for object specific data.
        //
        DMF_Platform_Free(platformObject->Data);
        // Free the object's critical section.
        //
        DMF_Platform_CriticalSectionDelete(&platformObject->ChildListLock);
        DMF_Platform_CriticalSectionDelete(&platformObject->ContextListLock);
        // Free the object's main memory.
        //
        DMF_Platform_Free(platformObject);
    }
}

DMF_PLATFORM_OBJECT*
DmfPlatformObjectCreate(
    _In_ DMF_PLATFORM_OBJECT* Parent,
    _In_ DMF_Platform_ObjectDelete ObjectDeleteCallback
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    platformObject = (DMF_PLATFORM_OBJECT*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_OBJECT));
    if (platformObject == NULL)
    {
        goto Exit;
    }

    // TODO: Use this.
    //
    platformObject->ReferenceCount = 1;

    // Set object delete callback. It deletes platform specific constructs
    // allocated when the object is created.
    //
    platformObject->ObjectDelete = ObjectDeleteCallback;

    // Initialize the list of contexts.
    //
    InitializeListHead(&platformObject->ContextList);
    DMF_Platform_CriticalSectionCreate(&platformObject->ContextListLock);

    // Initialize the list of children.
    //
    InitializeListHead(&platformObject->ChildList);
    DMF_Platform_CriticalSectionCreate(&platformObject->ChildListLock);
    
    DmfAssert(platformObject->ChildListEntry.Flink == NULL);
    DmfAssert(platformObject->ChildListEntry.Blink == NULL);

    // Initialize this object as not a child.
    //
    InitializeListHead(&platformObject->ChildListEntry);

    if (Parent != NULL)
    {
        DMF_Platform_CriticalSectionEnter(&Parent->ChildListLock);
        InsertTailList(&Parent->ChildList,
                       &platformObject->ChildListEntry);
        Parent->NumberOfChildren++;
        DMF_Platform_CriticalSectionLeave(&Parent->ChildListLock);
    }

Exit:

    return platformObject;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFMEMORY
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfMemoryDelete(
    DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    ASSERT(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeMemory);

    DMF_PLATFORM_MEMORY* platformMemory = (DMF_PLATFORM_MEMORY*)PlatformObject->Data;

    if (platformMemory->NeedToDeallocate)
    {
        DMF_Platform_Free(platformMemory->DataMemory);
    }
}

_Must_inspect_result_
_When_(PoolType == 1 || PoolType == 257, _IRQL_requires_max_(APC_LEVEL))
_When_(PoolType == 0 || PoolType == 256, _IRQL_requires_max_(DISPATCH_LEVEL))
NTSTATUS
DmfPlatform_WdfMemoryCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES Attributes,
    _In_
    _Strict_type_match_
    POOL_TYPE PoolType,
    _In_opt_
    ULONG PoolTag,
    _In_
    _When_(BufferSize == 0, __drv_reportError(BufferSize cannot be zero))
    size_t BufferSize,
    _Out_
    WDFMEMORY* Memory,
    _Outptr_opt_result_bytebuffer_(BufferSize)
    PVOID* Buffer
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(PoolType);
    UNREFERENCED_PARAMETER(PoolTag);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)Attributes->ParentObject;
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfMemoryDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_MEMORY*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_MEMORY));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeMemory;

    DMF_PLATFORM_MEMORY* platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    platformMemory->DataMemory = (CHAR*)DMF_Platform_Allocate(BufferSize);
    if (platformMemory->DataMemory == NULL)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformMemory->NeedToDeallocate = TRUE;
    platformMemory->Size = BufferSize;

    *Memory = (WDFMEMORY)platformObject;
    if (Buffer != NULL)
    {
        *Buffer = platformMemory->DataMemory;
    }

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfMemoryCreatePreallocated(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES Attributes,
    _In_ __drv_aliasesMem
    PVOID Buffer,
    _In_
    _When_(BufferSize == 0, __drv_reportError(BufferSize cannot be zero))
    size_t BufferSize,
    _Out_
    WDFMEMORY* Memory
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)Attributes->ParentObject;
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfMemoryDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_MEMORY*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_MEMORY));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    DMF_PLATFORM_MEMORY* platformMemory;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeMemory;

    platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;

    platformMemory->DataMemory = Buffer;
    platformMemory->NeedToDeallocate = FALSE;
    platformMemory->Size = BufferSize;

    memcpy(&platformObject->ObjectAttributes,
            Attributes,
            sizeof(WDF_OBJECT_ATTRIBUTES));
    *Memory = (WDFMEMORY)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
PVOID
DmfPlatform_WdfMemoryGetBuffer(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFMEMORY Memory,
    _Out_opt_
    size_t* BufferSize
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_MEMORY* platformMemory;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Memory;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeMemory);
    platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;
    if (BufferSize != NULL)
    {
        *BufferSize = platformMemory->Size;
    }

    return platformMemory->DataMemory;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFSYNC
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfWaitLockDelete(
    DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    ASSERT(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);

    DMF_PLATFORM_WAITLOCK* platformWaitLock = (DMF_PLATFORM_WAITLOCK*)PlatformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfWaitLockDelete_Win32(platformWaitLock);
#elif defined(DMF_XXX_MODE)
#endif
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfWaitLockCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES LockAttributes,
    _Out_
    WDFWAITLOCK* Lock
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;
    BOOLEAN waitEventCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (LockAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(LockAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfWaitLockDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_WAITLOCK*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_WAITLOCK));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeWaitLock;

    DMF_PLATFORM_WAITLOCK* platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    waitEventCreated = WdfWaitLockCreate_Win32(platformWaitLock);
#else
    #error Must define WdfWaitLockCreate_Xxx() for platform.
#endif
    if (! waitEventCreated)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    if (LockAttributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
                LockAttributes,
                sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *Lock = (WDFWAITLOCK)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     LockAttributes);

Exit:

    return ntStatus;
}

_When_(Timeout == NULL, _IRQL_requires_max_(PASSIVE_LEVEL))
_When_(Timeout != NULL && *Timeout == 0, _IRQL_requires_max_(DISPATCH_LEVEL))
_When_(Timeout != NULL && *Timeout != 0, _IRQL_requires_max_(PASSIVE_LEVEL))
_Always_(_When_(Timeout == NULL, _Acquires_lock_(Lock)))
_When_(Timeout != NULL && return == STATUS_SUCCESS, _Acquires_lock_(Lock))
_When_(Timeout != NULL, _Must_inspect_result_)
NTSTATUS
DmfPlatform_WdfWaitLockAcquire(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    _Requires_lock_not_held_(_Curr_)
    WDFWAITLOCK Lock,
    _In_opt_
    PLONGLONG Timeout
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WAITLOCK* platformWaitLock;
    DWORD returnValue;
    LONGLONG timeoutMs;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Lock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);
    platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    if (Timeout == NULL)
    {
        timeoutMs = INFINITE;
    }
    else
    {
        timeoutMs = WDF_REL_TIMEOUT_IN_MS(*Timeout);
    }

#if defined(DMF_WIN32_MODE)
    returnValue = WdfWaitLockAcquire_Win32(platformWaitLock,
                                           (DWORD)timeoutMs);
#else
    #error Must define WdfTimerCreate_Xxx() for platform.
#endif
    if (returnValue == WAIT_OBJECT_0)
    {
        ntStatus = STATUS_SUCCESS;
    }
    else if (returnValue == WAIT_TIMEOUT)
    {
        ntStatus = STATUS_TIMEOUT;
    }
    else
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfWaitLockRelease(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    _Requires_lock_held_(_Curr_)
    _Releases_lock_(_Curr_)
    WDFWAITLOCK Lock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WAITLOCK* platformWaitLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Lock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);
    platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfWaitLockRelease_Win32(platformWaitLock);
#else
    #error Must define WdfTimerCreate_Xxx() for platform.
#endif
}

void
DmfPlatformWdfSpinLockDelete(
    DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    ASSERT(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);

    DMF_PLATFORM_SPINLOCK* platformSpinLock = (DMF_PLATFORM_SPINLOCK*)PlatformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfSpinLockDelete_Win32(platformSpinLock);
#elif defined(DMF_XXX_MODE)
#endif
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfSpinLockCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES SpinLockAttributes,
    _Out_
    WDFSPINLOCK* SpinLock
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;
    BOOLEAN spinLockCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(SpinLockAttributes);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (SpinLockAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(SpinLockAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfSpinLockDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_SPINLOCK*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_SPINLOCK));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeSpinLock;

    DMF_PLATFORM_SPINLOCK* platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    spinLockCreated = WdfSpinLockCreate_Win32(platformSpinLock);
#else
    #error Must define WdfspinLockCreate_Xxx() for platform.
#endif
    if (!spinLockCreated)
    {
        goto Exit;
    }

    if (SpinLockAttributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               SpinLockAttributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *SpinLock = (WDFSPINLOCK)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     SpinLockAttributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_raises_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfSpinLockAcquire(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    _Requires_lock_not_held_(_Curr_)
    _Acquires_lock_(_Curr_)
    _IRQL_saves_
    WDFSPINLOCK SpinLock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfSpinLockAcquire_Win32(platformSpinLock);
#else
    #error Must define WdfspinLockAcquire_Xxx() for platform.
#endif
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_min_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfSpinLockRelease(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    _Requires_lock_held_(_Curr_)
    _Releases_lock_(_Curr_)
    _IRQL_restores_
    WDFSPINLOCK SpinLock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfSpinLockRelease_Win32(platformSpinLock);
#else
    #error Must define WdfspinLockRelease_Xxx() for platform.
#endif
}

///////////////////////////////////////////////////////////////////////////////////
// WDFTIMER
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfTimerDelete(
    DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    ASSERT(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);

    DMF_PLATFORM_TIMER* platformTimer = (DMF_PLATFORM_TIMER*)PlatformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfTimerDelete_Win32(platformTimer);
#elif defined(DMF_XXX_MODE)
#endif
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfTimerCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDF_TIMER_CONFIG Config,
    _In_
    PWDF_OBJECT_ATTRIBUTES Attributes,
    _Out_
    WDFTIMER* Timer
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;
    BOOLEAN timerCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(Attributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfTimerDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_TIMER*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_TIMER));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeTimer;

    DMF_PLATFORM_TIMER* platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

#if defined(DMF_WIN32_MODE)
    timerCreated = WdfTimerCreate_Win32(platformTimer,
                                        platformObject);
#else
    #error Must define WdfTimerCreate_Xxx() for platform.
#endif
    if (! timerCreated)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    memcpy(&platformTimer->Config,
           Config,
           sizeof(WDF_TIMER_CONFIG));

    *Timer = (WDFTIMER)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DmfPlatform_WdfTimerStart(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFTIMER Timer,
    _In_
    LONGLONG DueTime
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    returnValue = WdfTimerStart_Win32(platformTimer,
                                      DueTime);
#else
    #error Must define WdfTimerStart_Xxx() for platform.
#endif

    // Always tell caller timer was not in queue.
    //
    return returnValue;
}

_When_(Wait == __true, _IRQL_requires_max_(PASSIVE_LEVEL))
_When_(Wait == __false, _IRQL_requires_max_(DISPATCH_LEVEL))
BOOLEAN
DmfPlatform_WdfTimerStop(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFTIMER Timer,
    _In_
    BOOLEAN Wait
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

#if defined(DMF_WIN32_MODE)
    returnValue = WdfTimerStop_Win32(platformTimer,
                                     Wait);
#else
    #error Must define WdfTimerStop_Xxx() for platform.
#endif

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfTimerGetParentObject(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFTIMER Timer
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    return platformObject->ObjectAttributes.ParentObject;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFWORKITEM
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfWorkItemDelete(
    DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    ASSERT(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);

    DMF_PLATFORM_WORKITEM* platformWorkItem = (DMF_PLATFORM_WORKITEM*)PlatformObject->Data;

#if defined(DMF_WIN32_MODE)
    WdfWorkItemDelete_Win32(platformWorkItem);
#elif defined(DMF_XXX_MODE)
#endif
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfWorkItemCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDF_WORKITEM_CONFIG Config,
    _In_
    PWDF_OBJECT_ATTRIBUTES Attributes,
    _Out_
    WDFWORKITEM* WorkItem
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;
    BOOLEAN workitemCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(Attributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject,
                                             DmfPlatformWdfWorkItemDelete);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_WORKITEM*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_WORKITEM));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeWorkItem;

    DMF_PLATFORM_WORKITEM* platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

#if defined(DMF_WIN32_MODE)
    workitemCreated = WdfWorkItemCreate_Win32(platformWorkItem,
                                              platformObject);
#else
    #error Must define WdfTimerStop_Xxx() for platform.
#endif
    if (!workitemCreated)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    memcpy(&platformWorkItem->Config,
           Config,
           sizeof(WDF_WORKITEM_CONFIG));

    *WorkItem = (WDFWORKITEM)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfWorkItemEnqueue(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    // Cause the workitem callback to execute as soon as possible.
    //
#if defined(DMF_WIN32_MODE)
    WdfWorkItemEnqueue_Win32(platformWorkItem);
#else
    #error Must define WdfTimerStop_Xxx() for platform.
#endif
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfWorkItemGetParentObject(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    return platformObject->ObjectAttributes.ParentObject;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfPlatform_WdfWorkItemFlush(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    // Cause the workitem callback to execute as soon as possible.
    //
    WdfTimerStop(platformWorkItem->Timer,
                 TRUE);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFCOLLECTION
///////////////////////////////////////////////////////////////////////////////////
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfCollectionCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES CollectionAttributes,
    _Out_
    WDFCOLLECTION* Collection
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (CollectionAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(CollectionAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    // TODO: It seems we don't need to delete anything specifically.
    //
    platformObject = DmfPlatformObjectCreate(parentObject,
                                             NULL);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_COLLECTION*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_COLLECTION));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeCollection;

    DMF_PLATFORM_COLLECTION* platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WDF_OBJECT_ATTRIBUTES spinlockAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&spinlockAttributes);
    ntStatus = WdfSpinLockCreate(&spinlockAttributes,
                                 &platformCollection->ListLock);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    InitializeListHead(&platformCollection->List);
    platformCollection->ItemCount = 0;

    if (CollectionAttributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               CollectionAttributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *Collection = (WDFCOLLECTION)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     CollectionAttributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DmfPlatform_WdfCollectionGetCount(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformcollection;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformcollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    return platformcollection->ItemCount;
}

typedef struct
{
    LIST_ENTRY ListEntry;
    WDFOBJECT Object;
} COLLECTION_ENTRY;

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfCollectionAdd(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection,
    _In_
    WDFOBJECT Object
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    collectionEntry = (COLLECTION_ENTRY*)DMF_Platform_Allocate(sizeof(COLLECTION_ENTRY));
    if (NULL == collectionEntry)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    collectionEntry->Object = Object;

    WdfSpinLockAcquire(platformCollection->ListLock);

    InitializeListHead(&collectionEntry->ListEntry);

    InsertTailList(&platformCollection->List,
                   &collectionEntry->ListEntry);
    platformCollection->ItemCount++;

    WdfSpinLockRelease(platformCollection->ListLock);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfCollectionRemove(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection,
    _In_
    WDFOBJECT Item
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        if (collectionEntry->Object == Item)
        {
            RemoveEntryList(listEntry);
            platformCollection->ItemCount--;
            break;
        }
        else
        {
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfCollectionRemoveItem(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection,
    _In_
    ULONG Index
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;
    ULONG currentItemIndex;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    currentItemIndex = 0;
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        if (currentItemIndex == Index)
        {
            collectionEntry = CONTAINING_RECORD(listEntry,
                                                COLLECTION_ENTRY,
                                                ListEntry);
            RemoveEntryList(listEntry);
            platformCollection->ItemCount--;
            break;
        }
        else
        {
            currentItemIndex++;
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetItem(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection,
    _In_
    ULONG Index
    )
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;
    ULONG currentItemIndex;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    currentItemIndex = 0;
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        if (currentItemIndex == Index)
        {
            collectionEntry = CONTAINING_RECORD(listEntry,
                                                COLLECTION_ENTRY,
                                                ListEntry);
            returnValue = collectionEntry->Object;
            break;
        }
        else
        {
            currentItemIndex++;
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetFirstItem(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection
    )
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Flink;
    if (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        returnValue = collectionEntry->Object;
    }
    else
    {
        returnValue = NULL;
    }

    WdfSpinLockRelease(platformCollection->ListLock);
    
    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetLastItem(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFCOLLECTION Collection
    )
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Blink;
    if (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        returnValue = collectionEntry->Object;
    }
    else
    {
        returnValue = NULL;
    }

    WdfSpinLockRelease(platformCollection->ListLock);
    
    return returnValue;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFDEVICE
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetPnpPowerEventCallbacks(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_PNPPOWER_EVENT_CALLBACKS PnpPowerEventCallbacks
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PnpPowerEventCallbacks);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetPowerPolicyEventCallbacks(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_POWER_POLICY_EVENT_CALLBACKS PowerPolicyEventCallbacks
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PowerPolicyEventCallbacks);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfPlatform_WdfDeviceCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _Inout_
    PWDFDEVICE_INIT* DeviceInit,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES DeviceAttributes,
    _Out_
    WDFDEVICE* Device
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (DeviceAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(DeviceAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    // TODO: It seems we don't need to delete anything specifically.
    //
    platformObject = DmfPlatformObjectCreate(parentObject,
                                             NULL);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_DEVICE*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_DEVICE));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeDevice;

    // TODO:
    // DMF_PLATFORM_DEVICE* platformDevice = (DMF_PLATFORM_DEVICE*)platformObject->Data;
    //

    if (DeviceAttributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               DeviceAttributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *Device = (WDFDEVICE)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     DeviceAttributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetFileObjectConfig(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_FILEOBJECT_CONFIG FileObjectConfig,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES FileObjectAttributes
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(FileObjectConfig);
    UNREFERENCED_PARAMETER(FileObjectAttributes);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetCharacteristics(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    ULONG DeviceCharacteristics,
    _In_
    BOOLEAN OrInValues
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(DeviceCharacteristics);
    UNREFERENCED_PARAMETER(OrInValues);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetDeviceClass(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    CONST GUID* DeviceClassGuid
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(DeviceClassGuid);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFIOQUEUE
///////////////////////////////////////////////////////////////////////////////////
//

// NOTE: There is only dummy support for create function to prevent need to make
//       many changes in the rest of the code.
//       Once there is a platform that can make use of a queue, then this support
//       can be added.
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfIoQueueCreate(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFDEVICE Device,
    _In_
    PWDF_IO_QUEUE_CONFIG Config,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES QueueAttributes,
    _Out_opt_
    WDFQUEUE* Queue
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Device);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (QueueAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(QueueAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    // TODO: It seems we don't need to delete anything specifically.
    //
    platformObject = DmfPlatformObjectCreate(parentObject,
                                             NULL);
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->Data = (DMF_PLATFORM_QUEUE*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_QUEUE));
    if (platformObject->Data == NULL)
    {
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformObject->PlatformObjectType = DmfPlatformObjectTypeQueue;

    DMF_PLATFORM_QUEUE* platformQueue = (DMF_PLATFORM_QUEUE*)platformObject->Data;

    memcpy(&platformQueue->Config,
           Config,
           sizeof(WDF_IO_QUEUE_CONFIG));

    memcpy(&platformObject->ObjectAttributes,
           QueueAttributes,
           sizeof(WDF_OBJECT_ATTRIBUTES));
    *Queue = (WDFQUEUE)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DmfPlatform_WdfIoQueueGetDevice(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFQUEUE Queue
    )
{
    // TODO:
    //
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Queue);
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFREQUEST
///////////////////////////////////////////////////////////////////////////////////
//


_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfRequestComplete(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFREQUEST Request,
    _In_
    NTSTATUS Status
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFFILEOBJECT
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DmfPlatform_WdfFileObjectGetDevice(
    _In_
    PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFFILEOBJECT FileObject
    )
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(FileObject);
    return NULL;
}

#elif defined(DMF_USER_MODE)
    // Use WDF directly.
    //
#elif !defined(DMF_USER_MODE)
    // Use WDF directly.
    //
#endif

///////////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlaformNotImplemented(
    void
    )
{
    DmfAssert(FALSE);
}

// Table of all function pointers.
//
WDFFUNC WdfFunctions[WdfFunctionTableNumEntries] = 
{
    // Defaults to NULL.
    //
};

WDF_DRIVER_GLOBALS WdfDriverGlobalsBuffer;
WDF_DRIVER_GLOBALS* WdfDriverGlobals = &WdfDriverGlobalsBuffer;
// FALSE = "All functions are always available".
//
BOOLEAN WdfClientVersionHigherThanFramework = FALSE;
ULONG WdfFunctionCount= WdfFunctionTableNumEntries;
ULONG WdfStructureCount= WDF_STRUCTURE_TABLE_NUM_ENTRIES;

void
DMF_PlatformInitialize(
    void
    )
{
    ULONG functionTableIndex;

    // Set default function for all functions to "not implemented".
    //
    for (functionTableIndex = 0; functionTableIndex < WdfFunctionTableNumEntries; functionTableIndex++)
    {
        switch (functionTableIndex)
        {
            // WDFSYNC
            //
            case WdfWaitLockCreateTableIndex:
                WdfFunctions[WdfWaitLockCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockCreate;
                break;
            case WdfWaitLockAcquireTableIndex:
                WdfFunctions[WdfWaitLockAcquireTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockAcquire;
                break;
            case WdfWaitLockReleaseTableIndex:
                WdfFunctions[WdfWaitLockReleaseTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockRelease;
                break;
            case WdfSpinLockCreateTableIndex:
                WdfFunctions[WdfSpinLockCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockCreate;
                break;
            case WdfSpinLockAcquireTableIndex:
                WdfFunctions[WdfSpinLockAcquireTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockAcquire;
                break;
            case WdfSpinLockReleaseTableIndex:
                WdfFunctions[WdfSpinLockReleaseTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockRelease;
                break;
            // WDFOBJECT
            //
            case WdfObjectDeleteTableIndex:
                WdfFunctions[WdfObjectDeleteTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectDelete;
                break;
            case WdfObjectGetTypedContextWorkerTableIndex:
                WdfFunctions[WdfObjectGetTypedContextWorkerTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectGetTypedContextWorker;
                break;
            case WdfObjectAllocateContextTableIndex:
                WdfFunctions[WdfObjectAllocateContextTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectAllocateContext;
                break;
            // WDFDEVICE
            //
            case WdfDeviceCreateTableIndex:
                WdfFunctions[WdfDeviceCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceCreate;
                break;
            case WdfDeviceInitSetPnpPowerEventCallbacksTableIndex:
                WdfFunctions[WdfDeviceInitSetPnpPowerEventCallbacksTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetPnpPowerEventCallbacks;
                break;
            case WdfDeviceInitSetPowerPolicyEventCallbacksTableIndex:
                WdfFunctions[WdfDeviceInitSetPowerPolicyEventCallbacksTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetPowerPolicyEventCallbacks;
                break;
            // WDFMEMORY
            //
            case WdfMemoryCreateTableIndex:
                WdfFunctions[WdfMemoryCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryCreate;
                break;
            case WdfMemoryCreatePreallocatedTableIndex:
                WdfFunctions[WdfMemoryCreatePreallocatedTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryCreatePreallocated;
                break;
            case WdfMemoryGetBufferTableIndex:
                WdfFunctions[WdfMemoryGetBufferTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryGetBuffer;
                break;
            // WDFIOQUEUE
            //
            case WdfIoQueueCreateTableIndex:
                WdfFunctions[WdfIoQueueCreateTableIndex] = (WDFFUNC)WdfIoQueueCreate;
                break;
            case WdfIoQueueGetDeviceTableIndex:
                WdfFunctions[WdfIoQueueGetDeviceTableIndex] = (WDFFUNC)DmfPlatform_WdfIoQueueGetDevice;
                break;
            // WDFFILEOBJECT
            //
            case WdfFileObjectGetDeviceTableIndex:
                WdfFunctions[WdfFileObjectGetDeviceTableIndex] = (WDFFUNC)DmfPlatform_WdfFileObjectGetDevice;
                break;
            case WdfDeviceInitSetFileObjectConfigTableIndex:
                WdfFunctions[WdfDeviceInitSetFileObjectConfigTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetFileObjectConfig;
                break;
            // WDFCOLLECTION
            //
            case WdfCollectionCreateTableIndex:
                WdfFunctions[WdfCollectionCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionCreate;
                break;
            case WdfCollectionGetCountTableIndex:
                WdfFunctions[WdfCollectionGetCountTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetCount;
                break;
            case WdfCollectionAddTableIndex:
                WdfFunctions[WdfCollectionAddTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionAdd;
                break;
            case WdfCollectionRemoveTableIndex:
                WdfFunctions[WdfCollectionRemoveTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionRemove;
                break;
            case WdfCollectionRemoveItemTableIndex:
                WdfFunctions[WdfCollectionRemoveItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionRemoveItem;
                break;
            case WdfCollectionGetItemTableIndex:
                WdfFunctions[WdfCollectionGetItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetItem;
                break;
            case WdfCollectionGetFirstItemTableIndex:
                WdfFunctions[WdfCollectionGetFirstItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetLastItem;
                break;
            case WdfCollectionGetLastItemTableIndex:
                WdfFunctions[WdfCollectionGetLastItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetLastItem;
                break;
            // WDFTIMER
            //
            case WdfTimerCreateTableIndex:
                WdfFunctions[WdfTimerCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerCreate;
                break;
            case WdfTimerStartTableIndex:
                WdfFunctions[WdfTimerStartTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerStart;
                break;
            case WdfTimerStopTableIndex:
                WdfFunctions[WdfTimerStopTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerStop;
                break;
            case WdfTimerGetParentObjectTableIndex:
                WdfFunctions[WdfTimerGetParentObjectTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerGetParentObject;
                break;
            // WDFWORKITEM
            //
            case WdfWorkItemCreateTableIndex:
                WdfFunctions[WdfWorkItemCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemCreate;
                break;
            case WdfWorkItemEnqueueTableIndex:
                WdfFunctions[WdfWorkItemEnqueueTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemEnqueue;
                break;
            case WdfWorkItemGetParentObjectTableIndex:
                WdfFunctions[WdfWorkItemGetParentObjectTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemGetParentObject;
                break;
            case WdfWorkItemFlushTableIndex:
                WdfFunctions[WdfWorkItemFlushTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemFlush;
                break;
            // NOT IMPLEMENTED
            //
            default:
            {
                WdfFunctions[functionTableIndex] = DmfPlaformNotImplemented;
                break;
            }
        }
    }
}

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.c
//
