/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform_Win32.c

Abstract:

    Win32 platform support for lower edge of DMF.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Win32 Application

--*/

#include "DmfIncludeInternal.h"

#include "DmfPlatform.tmh"

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

////////////////////////////////////////////////////////////////////////////
// Win32 Primitives for Memory Allocation
////////////////////////////////////////////////////////////////////////////
//

#if !defined(DMF_WIN32_MODE)
#error Must defined DMF_WIN32_MODE to include this file.
#endif

void*
DMF_Platform_Allocate(
    _In_ size_t Size
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
    _In_ void* Pointer
    )
{
    free(Pointer);
}

////////////////////////////////////////////////////////////////////////////
// Win32 Primitives for Timer, Workitem, Locks
////////////////////////////////////////////////////////////////////////////
//

////////////////////////////////////////////////////////////////////////////
// Critical Section
//

typedef CRITICAL_SECTION DMF_PLATFORM_CRITICAL_SECTION;

BOOLEAN
DMF_Platform_CriticalSectionCreate(
    _Inout_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    )
{
    BOOLEAN returnValue;

    InitializeCriticalSection(CriticalSection);

    returnValue = TRUE;

    return returnValue;
}

_Acquires_lock_(* CriticalSection)
void
DMF_Platform_CriticalSectionEnter(
    _Inout_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    )
{
    EnterCriticalSection(CriticalSection);
}

_Releases_lock_(* CriticalSection)
void
DMF_Platform_CriticalSectionLeave(
    _Inout_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    )
{
    LeaveCriticalSection(CriticalSection);
}

void
DMF_Platform_CriticalSectionDelete(
    _Inout_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    )
{
    DeleteCriticalSection(CriticalSection);
}

////////////////////////////////////////////////////////////////////////////
// Timer
//

// 'The pointer argument 'Instance' for function 'TimerCallback' can be marked as a pointer to const (con.3).'
//
#pragma warning(suppress:26461)
VOID 
CALLBACK 
TimerCallback(
    _In_ PTP_CALLBACK_INSTANCE Instance,
    _In_ PVOID Parameter,
    _In_ PTP_TIMER Timer
    )
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;

    UNREFERENCED_PARAMETER(Timer);
    UNREFERENCED_PARAMETER(Instance);

    // Parameter should never be NULL. If it is, let it blow up and let us fix it.
    //
    platformObject = static_cast<DMF_PLATFORM_OBJECT*>(Parameter);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    platformTimer->Config.EvtTimerFunc((WDFTIMER)platformObject);

    if (platformTimer->Config.Period > 0)
    {
        WdfTimerStart((WDFTIMER)platformObject,
                      platformTimer->Config.Period);
    }
}

BOOLEAN
WdfTimerCreate_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    BOOLEAN returnValue;

    PlatformTimer->PtpTimer = CreateThreadpoolTimer(TimerCallback,
                                                    (PVOID)PlatformObject,
                                                    nullptr);
    if (PlatformTimer->PtpTimer != NULL)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
WdfTimerStart_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ LONGLONG DueTime
    )
{
    ULARGE_INTEGER dueTimeInteger;
    FILETIME dueTimeFile;

    dueTimeInteger.QuadPart = DueTime;
    dueTimeFile.dwHighDateTime = dueTimeInteger.HighPart;
    dueTimeFile.dwLowDateTime = dueTimeInteger.LowPart;

    SetThreadpoolTimer(PlatformTimer->PtpTimer,
                       &dueTimeFile,
                       0,
                       0);

    // Always tell caller timer was not in queue.
    //
    return FALSE;
}

BOOLEAN
WdfTimerStop_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ BOOLEAN Wait
    )
{
    SetThreadpoolTimer(PlatformTimer->PtpTimer,
                       NULL,
                       0,
                       0);
    if (Wait)
    {
        WaitForThreadpoolTimerCallbacks(PlatformTimer->PtpTimer, 
                                        TRUE);
    }

    return TRUE;
}

void
WdfTimerDelete_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer
    )
{
    WdfTimerStop_Win32(PlatformTimer,
                       TRUE);
    CloseThreadpoolTimer(PlatformTimer->PtpTimer);
}

////////////////////////////////////////////////////////////////////////////
// Workitem
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

BOOLEAN
WdfWorkItemCreate_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
{
    BOOLEAN returnValue;
    WDF_TIMER_CONFIG timerConfig;
    NTSTATUS ntStatus;

    WDF_TIMER_CONFIG_INIT(&timerConfig, 
                          WorkitemCallback);
    ntStatus = WdfTimerCreate(&timerConfig,
                              &PlatformObject->ObjectAttributes,
                              &PlatformWorkItem->Timer);
    if (!NT_SUCCESS(ntStatus))
    {
        returnValue = FALSE;
        goto Exit;
    }

    returnValue = TRUE;

Exit:

    return returnValue;
}

BOOLEAN
WdfWorkItemEnqueue_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
{
    WdfTimerStart(PlatformWorkItem->Timer,
                  0);
    return TRUE;
}

void
WdfWorkItemFlush_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
{
    WdfTimerStop(PlatformWorkItem->Timer,
                 TRUE);
}

void
WdfWorkItemDelete_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
{
    WdfTimerStop(PlatformWorkItem->Timer,
                 TRUE);
}

////////////////////////////////////////////////////////////////////////////
// Waitlock
//

BOOLEAN
WdfWaitLockCreate_Win32(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
{
    BOOLEAN returnValue;

    PlatformWaitLock->Event = CreateEvent(NULL,
                                          FALSE,
                                          TRUE,
                                          NULL);
    if (PlatformWaitLock->Event != NULL)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

DWORD
WdfWaitLockAcquire_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock,
    _In_ DWORD TimeoutMs
    )
{
    DWORD returnValue;

    returnValue = WaitForSingleObject(PlatformWaitLock->Event,
                                      (DWORD)TimeoutMs);

    return returnValue;
}

VOID
WdfWaitLockRelease_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
{
    SetEvent(PlatformWaitLock->Event);
}

void
WdfWaitLockDelete_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
{
    CloseHandle(PlatformWaitLock->Event);
    PlatformWaitLock->Event = NULL;
}

////////////////////////////////////////////////////////////////////////////
// SpinLock
//

BOOLEAN
WdfSpinLockCreate_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
{
    BOOLEAN returnValue;

    InitializeCriticalSection(&PlatformSpinLock->SpinLock);

    returnValue = TRUE;

    return returnValue;
}

_Acquires_lock_(PlatformSpinLock->SpinLock)
VOID
WdfSpinLockAcquire_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
{
    EnterCriticalSection(&PlatformSpinLock->SpinLock);
}

_Releases_lock_(PlatformSpinLock->SpinLock) 
VOID
WdfSpinLockRelease_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
{
    LeaveCriticalSection(&PlatformSpinLock->SpinLock);
}

void
WdfSpinLockDelete_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
{
    DeleteCriticalSection(&PlatformSpinLock->SpinLock);
}

void
PlatformInitialize(
    void
    )
{
    // NOTE: Platform can start deamon or thread or allocate resources
    //       that are stored in global memory.
    //
}

void
PlatformUninitialize(
    void
    )
{
    // NOTE: Platform does inverse of what it did above.
    //
}

// NOTE: This exact name is defined in each platform, but only a single 
//       instance is compiled.
//
DmfPlatform_Handlers DmfPlatformHandlersTable =
{
    PlatformInitialize,
    PlatformUninitialize,
    WdfTimerCreate_Win32,
    WdfTimerStart_Win32,
    WdfTimerStop_Win32,
    WdfTimerDelete_Win32,
    WdfWorkItemCreate_Win32,
    WdfWorkItemEnqueue_Win32,
    WdfWorkItemFlush_Win32,
    WdfWorkItemDelete_Win32,
    WdfWaitLockCreate_Win32,
    WdfWaitLockAcquire_Win32,
    WdfWaitLockRelease_Win32,
    WdfWaitLockDelete_Win32,
    WdfSpinLockCreate_Win32,
    WdfSpinLockAcquire_Win32,
    WdfSpinLockRelease_Win32,
    WdfSpinLockDelete_Win32,
};

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.c
//
