/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform_Win32.h

Abstract:

    Companion file to DmfPlatform_Win32.c.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Win32 Application

--*/

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

////////////////////////////////////////////////////////////////////////////
// Win32 Primitives for Memory Allocation
////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_WIN32_MODE)

typedef struct
{
    LIST_ENTRY ListEntry;
    WDF_OBJECT_CONTEXT_TYPE_INFO ContextTypeInfo;
    VOID* ContextData;
} DMF_PLATFORM_CONTEXT;

typedef struct
{
    void* DataMemory;
    size_t Size;
    BOOLEAN NeedToDeallocate;
} DMF_PLATFORM_MEMORY;

typedef struct
{
    CRITICAL_SECTION SpinLock;
} DMF_PLATFORM_SPINLOCK;

typedef struct
{
    HANDLE Event;
} DMF_PLATFORM_WAITLOCK;

typedef struct
{
    PTP_TIMER PtpTimer;
    WDF_TIMER_CONFIG Config;
} DMF_PLATFORM_TIMER;

typedef struct
{
    // Use timer to caused deferred processing in Win32.
    //
    WDFTIMER Timer;
    WDF_WORKITEM_CONFIG Config;
} DMF_PLATFORM_WORKITEM;

typedef struct
{
    LIST_ENTRY List;
    ULONG ItemCount;
    WDFSPINLOCK ListLock;
} DMF_PLATFORM_COLLECTION;

typedef struct
{
    WDF_IO_QUEUE_CONFIG Config;
} DMF_PLATFORM_QUEUE;

typedef struct
{
    ULONG Dummy;
} DMF_PLATFORM_DEVICE;

#define PAGED_CODE()

void*
DMF_Platform_Allocate(
    size_t Size
    );

void
DMF_Platform_Free(
    void* Pointer
    );

typedef CRITICAL_SECTION DMF_PLATFORM_CRITICAL_SECTION;

BOOLEAN
DMF_Platform_CriticalSectionCreate(
    _Out_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    );

void
DMF_Platform_CriticalSectionEnter(
    _Out_ DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    );

void
DMF_Platform_CriticalSectionLeave(
    DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    );

void
DMF_Platform_CriticalSectionDelete(
    DMF_PLATFORM_CRITICAL_SECTION* CriticalSection
    );

BOOLEAN
WdfTimerCreate_Win32(
    _In_ DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
WdfTimerStart_Win32(
    _In_
    DMF_PLATFORM_TIMER* PlatformTimer,
    _In_
    LONGLONG DueTime
    );

BOOLEAN
WdfTimerStop_Win32(
    _In_
    DMF_PLATFORM_TIMER* PlatformTimer,
    _In_
    BOOLEAN Wait
    );

void
WdfTimerDelete_Win32(
    _In_ DMF_PLATFORM_TIMER* PlatformTimer
    );

BOOLEAN
WdfWorkItemCreate_Win32(
    _In_ DMF_PLATFORM_WORKITEM* PlatformWorkItem,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
WdfWorkItemEnqueue_Win32(
    _In_
    DMF_PLATFORM_WORKITEM* PlatformWorkItem
    );

void
WdfWorkItemDelete_Win32(
    _In_ DMF_PLATFORM_WORKITEM* PlatformWorkItem
    );

BOOLEAN
WdfWaitLockCreate_Win32(
    _Out_ DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

DWORD
WdfWaitLockAcquire_Win32(
    _Out_ DMF_PLATFORM_WAITLOCK* PlatformWaitLock,
    _In_ DWORD TimeoutMs
    );

VOID
WdfWaitLockRelease_Win32(
    _Out_ DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

void
WdfWaitLockDelete_Win32(
    _Out_ DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

BOOLEAN
WdfSpinLockCreate_Win32(
    _Out_ DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

VOID
WdfSpinLockAcquire_Win32(
    _Out_ DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

VOID
WdfSpinLockRelease_Win32(
    _Out_ DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

void
WdfSpinLockDelete_Win32(
    _Out_ DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

#endif // defined(DMF_WIN32_MODE)

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform_Win32.h
//
