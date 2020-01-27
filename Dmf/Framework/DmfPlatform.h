/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform.h

Abstract:

    Companion file to DmfPlatform.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework
    Win32 Application

--*/

#pragma once

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

#if !defined(DMF_USER_MODE)
    // Windows Kernel-mode supported internally.
    //
#endif

#if defined(DMF_USER_MODE)
    // Windows User-mode supported internally.
    //
#endif

// Common definitions for custom platforms.
//

#if defined(DMF_WIN32_MODE)

typedef void* PWDF_DRIVER_GLOBALS;

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) 
#define STATUS_ABANDONED                 ((NTSTATUS)0x00000080L)
#define STATUS_ALERTED                   ((NTSTATUS)0x00000101L)
#if !defined(STATUS_TIMEOUT)
    // TODO: I don't understand this. Need to fix it.
    //
    #define STATUS_TIMEOUT                   ((NTSTATUS)0x00000102L)
#endif
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL) 
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_COLLISION     ((NTSTATUS)0xC0000035L)
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBL)
#if !defined(STATUS_INVALID_PARAMETER)
    // TODO: I don't understand this. Need to fix it.
    //
    #define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_INTERNAL_ERROR            ((NTSTATUS)0xC00000E5L)
#define STATUS_CANCELLED                 ((NTSTATUS)0xC0000120L)
#define STATUS_INVALID_DEVICE_STATE      ((NTSTATUS)0xC0000184L)
#define STATUS_DEVICE_PROTOCOL_ERROR     ((NTSTATUS)0xC0000186L)
#define STATUS_INVALID_BUFFER_SIZE       ((NTSTATUS)0xC0000206L)
#define STATUS_NOT_FOUND                 ((NTSTATUS)0xC0000225L)
#define NT_SUCCESS(Status)  (((NTSTATUS)(Status)) >= 0)

#define STATUS_WAIT_0                    ((DWORD   )0x00000000L) 
#define STATUS_WAIT_1                    (STATUS_WAIT_0 + 1)

#include ".\Platform\wudfwdm.h"
#include ".\Platform\wdftypes.h"
#include ".\Platform\wdfobject.h"
#include ".\Platform\wdfcore.h"

#define WDF_DEVICE_NO_WDMSEC_H
#include ".\Platform\wdfdevice.h"

#include ".\Platform\wdfmemory.h"
#include ".\Platform\wdfsync.h"
#include ".\Platform\wdftimer.h"
#include ".\Platform\wdfworkitem.h"
#include ".\Platform\wdfcollection.h"
#include ".\Platform\wdfio.h"
#include ".\Platform\wdfrequest.h"
#include ".\Platform\wdffileobject.h"

#define PAGED_CODE()

typedef enum
{
    DmfPlatformObjectTypeUndefined,
    DmfPlatformObjectTypeMemory,
    DmfPlatformObjectTypeWaitLock,
    DmfPlatformObjectTypeSpinLock,
    DmfPlatformObjectTypeTimer,
    DmfPlatformObjectTypeWorkItem,
    DmfPlatformObjectTypeCollection,
    DmfPlatformObjectTypeDevice,
    DmfPlatformObjectTypeQueue,
} DmfPlatformObjectType;

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

// NOTE: This is analogous to WDFOBJECT.
//
typedef struct
{
    DmfPlatformObjectType PlatformObjectType;
    // Pointer to:
    //    DMF_PLATFORM_MEMORY
    //    DMF_PLATFORM_WAITLOCK
    //    DMF_PLATFORM_SPINLOCK
    //    DMF_PLATFORM_TIMER
    //    DMF_PLATFORM_WORKITEM
    //    DMF_PLATFORM_COLLECTION
    //    DMF_PLATFORM_DEVICE
    //    DMF_PLATFORM_QUEUE
    //
    void* Data;
    WDF_OBJECT_ATTRIBUTES ObjectAttributes;
    // Maintains a list of child objects.
    //
    LIST_ENTRY ChildList;
    CRITICAL_SECTION ChildListLock;
    // Allows this structure to be inserted into the parent's list
    // of child objects so that the tree of objects can be deleted
    // automatically when the parent is deleted.
    //
    LIST_ENTRY ChildListEntry;
    // Maintains a list of contexts.
    //
    LIST_ENTRY ContextList;
} DMF_PLATFORM_OBJECT;

#endif

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.h
//