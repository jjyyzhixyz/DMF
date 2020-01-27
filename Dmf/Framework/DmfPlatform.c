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

#if defined(DMF_WIN32_MODE)

void*
DMF_Platform_Allocate(
    size_t Size
    )
{
    void* returnValue;
    returnValue = malloc(Size);
    if (returnValue != NULL)
    {
        ZeroMemory(returnValue,
                   Size);
    }

    return returnValue;
}

void
DMF_Platform_Free(
    void* Pointer
    )
{
    free(Pointer);
}

#elif defined(DMF_USER_MODE)
    // Use native primitives.
    //
#elif !defined(DMF_USER_MODE)
    // Use native primitives.
    //
#endif

// These are used for all custom platforms
//

#if defined(DMF_WIN32_MODE)

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
WdfObjectAllocateContext(
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

    platformObject = (DMF_PLATFORM_OBJECT*)Handle;

    platformContext = (DMF_PLATFORM_CONTEXT*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_CONTEXT));
    if (platformContext == NULL)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    platformContext->ContextData = DMF_Platform_Allocate(ContextAttributes->Size);
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

    InsertTailList(&platformObject->ContextList,
                   &platformContext->ListEntry);

    printf("\nAdd Context[%s] to 0x%p", platformContext->ContextTypeInfo.ContextName, platformObject);

    if (Context != NULL)
    {
        *Context = platformContext->ContextData;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

PVOID
WdfObjectGetTypedContextWorker(
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

    returnValue = NULL;
    platformObject = (DMF_PLATFORM_OBJECT*)Handle;
    listEntry = platformObject->ContextList.Flink;
    printf("\nLookfor Context[%s] in 0x%p", TypeInfo->ContextName, platformObject);
    while (listEntry != &platformObject->ContextList)
    {
        platformContext = CONTAINING_RECORD(listEntry,
                                            DMF_PLATFORM_CONTEXT,
                                            ListEntry);
        if (TypeInfo->ContextName == platformContext->ContextTypeInfo.ContextName)
        {
            returnValue = platformContext->ContextData;
            printf("\nFound Context[%s] to 0x%p", platformContext->ContextTypeInfo.ContextName, platformObject);
            break;
        }
        else
        {
            printf("\nSkip Context[%s]", platformContext->ContextTypeInfo.ContextName);
        }
        listEntry = listEntry->Flink;
    }

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfObjectDelete(
    _In_
    WDFOBJECT Object
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* childObject;
    LIST_ENTRY* listEntry;

    platformObject = (DMF_PLATFORM_OBJECT*)Object;
    listEntry = platformObject->ChildList.Flink;
    while (listEntry != &platformObject->ChildList)
    {
        childObject = CONTAINING_RECORD(listEntry,
                                        DMF_PLATFORM_OBJECT,
                                        ChildListEntry);
        WdfObjectDelete(childObject);
        listEntry = listEntry->Flink;
    }

    // TODO: Call object specific deletion function.
    //
    DMF_Platform_Free(platformObject->Data);
    DMF_Platform_Free(platformObject);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFMEMORY
///////////////////////////////////////////////////////////////////////////////////
//

// TODO: Pass object type and the object specific deletion function.
//
DMF_PLATFORM_OBJECT*
DmfPlatformObjectCreate(
    DMF_PLATFORM_OBJECT* Parent
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    platformObject = (DMF_PLATFORM_OBJECT*)DMF_Platform_Allocate(sizeof(DMF_PLATFORM_OBJECT));
    if (platformObject == NULL)
    {
        goto Exit;
    }

    // Initialize the list of contexts.
    //
    InitializeListHead(&platformObject->ContextList);

    // Initialize the list of children.
    //
    InitializeListHead(&platformObject->ChildList);
    InitializeCriticalSection(&platformObject->ChildListLock);
    
    DmfAssert(platformObject->ChildListEntry.Flink == NULL);
    DmfAssert(platformObject->ChildListEntry.Blink == NULL);
    // Initialize this object as not a child.
    //
    InitializeListHead(&platformObject->ChildListEntry);

    if (Parent != NULL)
    {
#if 0
        EnterCriticalSection(&Parent->ChildListLock);
        printf("\nAdd Child[0x%p] to 0x%p", platformObject, Parent);
        InsertTailList(&Parent->ChildList,
                       &platformObject->ChildListEntry);
        LeaveCriticalSection(&Parent->ChildListLock);
#endif
    }

Exit:

    return platformObject;
}

_Must_inspect_result_
_When_(PoolType == 1 || PoolType == 257, _IRQL_requires_max_(APC_LEVEL))
_When_(PoolType == 0 || PoolType == 256, _IRQL_requires_max_(DISPATCH_LEVEL))
NTSTATUS
WdfMemoryCreate(
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

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_MEMORY* platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeMemory;

    platformMemory->DataMemory = (CHAR*)DMF_Platform_Allocate(BufferSize);
    if (platformMemory->DataMemory == NULL)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    platformMemory->NeedToDeallocate = TRUE;
    platformMemory->Size = BufferSize;

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
                Attributes,
                sizeof(WDF_OBJECT_ATTRIBUTES));
    }

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
WdfMemoryCreatePreallocated(
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

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatformObjectCreate((DMF_PLATFORM_OBJECT*)(Attributes->ParentObject));
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

    platformMemory = (DMF_PLATFORM_MEMORY*)platformObject;
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
WdfMemoryGetBuffer(
    _In_
    WDFMEMORY Memory,
    _Out_opt_
    size_t* BufferSize
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_MEMORY* platformMemory;

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

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
WdfWaitLockCreate(
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES LockAttributes,
    _Out_
    WDFWAITLOCK* Lock
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    UNREFERENCED_PARAMETER(LockAttributes);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (LockAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(LockAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_WAITLOCK* platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeWaitLock;
    platformWaitLock->Event = CreateEvent(NULL,
                                            FALSE,
                                            FALSE,
                                            NULL);
    if (platformWaitLock->Event == NULL)
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
WdfWaitLockAcquire(
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
    returnValue = WaitForSingleObject(platformWaitLock->Event,
                                      (DWORD)timeoutMs);
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
WdfWaitLockRelease(
    _In_
    _Requires_lock_held_(_Curr_)
    _Releases_lock_(_Curr_)
    WDFWAITLOCK Lock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WAITLOCK* platformWaitLock;

    platformObject = (DMF_PLATFORM_OBJECT*)Lock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);
    platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    SetEvent(platformWaitLock->Event);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
WdfSpinLockCreate(
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES SpinLockAttributes,
    _Out_
    WDFSPINLOCK* SpinLock
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

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

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_SPINLOCK* platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeSpinLock;

    InitializeCriticalSection(&platformSpinLock->SpinLock);

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
WdfSpinLockAcquire(
    _In_
    _Requires_lock_not_held_(_Curr_)
    _Acquires_lock_(_Curr_)
    _IRQL_saves_
    WDFSPINLOCK SpinLock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    EnterCriticalSection(&platformSpinLock->SpinLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_min_(DISPATCH_LEVEL)
VOID
WdfSpinLockRelease(
    _In_
    _Requires_lock_held_(_Curr_)
    _Releases_lock_(_Curr_)
    _IRQL_restores_
    WDFSPINLOCK SpinLock
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    LeaveCriticalSection(&platformSpinLock->SpinLock);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFTIMER
///////////////////////////////////////////////////////////////////////////////////
//

VOID 
CALLBACK 
TimerCallback(
    __in PTP_CALLBACK_INSTANCE Instance,
    __in_opt PVOID Parameter,
    __in PTP_TIMER Timer
    );

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
WdfTimerCreate(
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

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(Attributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_TIMER* platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeTimer;
    platformTimer->PtpTimer = CreateThreadpoolTimer(TimerCallback,
                                                    (PVOID)platformObject,
                                                    nullptr);
    if (platformTimer->PtpTimer == NULL)
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    memcpy(&platformTimer->Config,
           Config,
           sizeof(WDF_TIMER_CONFIG));

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *Timer = (WDFTIMER)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
WdfTimerStart(
    _In_
    WDFTIMER Timer,
    _In_
    LONGLONG DueTime
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;
    ULARGE_INTEGER dueTimeInteger;
    FILETIME dueTimeFile;

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    dueTimeInteger.QuadPart = DueTime;
    dueTimeFile.dwHighDateTime = dueTimeInteger.HighPart;
    dueTimeFile.dwLowDateTime = dueTimeInteger.LowPart;

    SetThreadpoolTimer(platformTimer->PtpTimer,
                       &dueTimeFile,
                       0,
                       0);

    // Always tell caller timer was not in queue.
    //
    return FALSE;
}

_When_(Wait == __true, _IRQL_requires_max_(PASSIVE_LEVEL))
_When_(Wait == __false, _IRQL_requires_max_(DISPATCH_LEVEL))
BOOLEAN
WdfTimerStop(
    _In_
    WDFTIMER Timer,
    _In_
    BOOLEAN Wait
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    SetThreadpoolTimer(platformTimer->PtpTimer,
                       NULL,
                       0,
                       0);
    if (Wait)
    {
        WaitForThreadpoolTimerCallbacks(platformTimer->PtpTimer, 
                                        TRUE);
    }

    return TRUE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
WdfTimerGetParentObject(
    _In_
    WDFTIMER Timer
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    return platformObject->ObjectAttributes.ParentObject;
}

VOID 
CALLBACK 
TimerCallback(
    __in PTP_CALLBACK_INSTANCE Instance,
    __in_opt PVOID Parameter,
    __in PTP_TIMER Timer
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;

    UNREFERENCED_PARAMETER(Timer);
    UNREFERENCED_PARAMETER(Instance);
        
    // Parameter should never be NULL. If it is, let it blow up and let us fix it.
    //
    platformObject = reinterpret_cast<DMF_PLATFORM_OBJECT*>(Parameter);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    platformTimer->Config.EvtTimerFunc((WDFTIMER)platformObject);

    if (platformTimer->Config.Period > 0)
    {
        WdfTimerStart((WDFTIMER)platformObject,
                      platformTimer->Config.Period);
    }
}

///////////////////////////////////////////////////////////////////////////////////
// WDFWORKITEM
///////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WorkitemCallback(
    _In_
    WDFTIMER Timer
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    platformObject = (DMF_PLATFORM_OBJECT*)WdfTimerGetParentObject(Timer);;
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;
    platformWorkItem->Config.EvtWorkItemFunc((WDFWORKITEM)platformObject);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
WdfWorkItemCreate(
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
    WDF_TIMER_CONFIG timerConfig;

    ntStatus = STATUS_UNSUCCESSFUL;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(Attributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_WORKITEM* platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    WDF_TIMER_CONFIG_INIT(&timerConfig, 
                          WorkitemCallback);
    platformObject->PlatformObjectType = DmfPlatformObjectTypeWorkItem;
    ntStatus = WdfTimerCreate(&timerConfig,
                              Attributes,
                              &platformWorkItem->Timer);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_Platform_Free(platformObject->Data);
        DMF_Platform_Free(platformObject);
        goto Exit;
    }

    memcpy(&platformWorkItem->Config,
           Config,
           sizeof(WDF_WORKITEM_CONFIG));

    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    *WorkItem = (WDFWORKITEM)platformObject;

    ntStatus = CustomContextAllocate(platformObject,
                                     Attributes);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfWorkItemEnqueue(
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    // Cause the workitem callback to execute as soon as possible.
    //
    WdfTimerStart(platformWorkItem->Timer,
                  0);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
WdfWorkItemGetParentObject(
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    return platformObject->ObjectAttributes.ParentObject;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
WdfWorkItemFlush(
    _In_
    WDFWORKITEM WorkItem
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

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
WdfCollectionCreate(
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES CollectionAttributes,
    _Out_
    WDFCOLLECTION* Collection
    )
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* parentObject;

    ntStatus = STATUS_UNSUCCESSFUL;

    if (CollectionAttributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)(CollectionAttributes->ParentObject);
    }
    else
    {
        parentObject = NULL;
    }

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    DMF_PLATFORM_COLLECTION* platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeCollection;

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
WdfCollectionGetCount(
    _In_
    WDFCOLLECTION Collection
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformcollection;

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
WdfCollectionAdd(
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
WdfCollectionRemove(
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
WdfCollectionRemoveItem(
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
WdfCollectionGetItem(
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
WdfCollectionGetFirstItem(
    _In_
    WDFCOLLECTION Collection
    )
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

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
WdfCollectionGetLastItem(
    _In_
    WDFCOLLECTION Collection
    )
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

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

//
// WDF Function: WdfDeviceInitSetPnpPowerEventCallbacks
//
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfDeviceInitSetPnpPowerEventCallbacks(
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_PNPPOWER_EVENT_CALLBACKS PnpPowerEventCallbacks
    )
{
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PnpPowerEventCallbacks);
}

//
// WDF Function: WdfDeviceInitSetPowerPolicyEventCallbacks
//
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfDeviceInitSetPowerPolicyEventCallbacks(
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_POWER_POLICY_EVENT_CALLBACKS PowerPolicyEventCallbacks
    )
{
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PowerPolicyEventCallbacks);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
WdfDeviceCreate(
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

    platformObject = DmfPlatformObjectCreate(parentObject);
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

    // TODO:
    // DMF_PLATFORM_DEVICE* platformDevice = (DMF_PLATFORM_DEVICE*)platformObject->Data;
    //

    platformObject->PlatformObjectType = DmfPlatformObjectTypeDevice;

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
WdfDeviceInitSetFileObjectConfig(
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    PWDF_FILEOBJECT_CONFIG FileObjectConfig,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES FileObjectAttributes
    )
{
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(FileObjectConfig);
    UNREFERENCED_PARAMETER(FileObjectAttributes);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfDeviceInitSetCharacteristics(
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    ULONG DeviceCharacteristics,
    _In_
    BOOLEAN OrInValues
    )
{
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(DeviceCharacteristics);
    UNREFERENCED_PARAMETER(OrInValues);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfDeviceInitSetDeviceClass(
    _In_
    PWDFDEVICE_INIT DeviceInit,
    _In_
    CONST GUID* DeviceClassGuid
    )
{
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
WdfIoQueueCreate(
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

    UNREFERENCED_PARAMETER(Device);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatformObjectCreate((DMF_PLATFORM_OBJECT*)(QueueAttributes->ParentObject));
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

    DMF_PLATFORM_QUEUE* platformQueue = (DMF_PLATFORM_QUEUE*)platformObject->Data;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeQueue;

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
WdfIoQueueGetDevice(
    _In_
    WDFQUEUE Queue
    )
{
    // TODO:
    //
    UNREFERENCED_PARAMETER(Queue);
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFREQUEST
///////////////////////////////////////////////////////////////////////////////////
//


_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WdfRequestComplete(
    _In_
    WDFREQUEST Request,
    _In_
    NTSTATUS Status
    )
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFFILEOBJECT
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
WdfFileObjectGetDevice(
    _In_
    WDFFILEOBJECT FileObject
    )
{
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

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.c
//