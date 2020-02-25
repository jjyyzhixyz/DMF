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

#if defined(DMF_WIN32_MODE)
    // Win32 Mode uses many of the User-mode APIs.
    //
    #define DMF_USER_MODE
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Compiler warning filters.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Allow DMF Modules to disable false positive SAL warnings easily when 
// compiled as C++ file.
//
// NOTE: Use with caution. First, verify there are no legitimate warnings. Then, use this option.
//
#if (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)
    // Disable some SAL warnings because they are false positives.
    // NOTE: The pointers are checked via ASSERTs.
    //

    // 'Dereferencing NULL pointer'.
    //
    #pragma warning(disable:6011)
    // '<argument> may be NULL'.
    //
    #pragma warning(disable:6387)
#endif // (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)

// 'warning C4201: nonstandard extension used: nameless struct/union'
//
#pragma warning(disable:4201)

// Check that the Windows version is Win10 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10))

// Check that the Windows version is RS3 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS3_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10_RS3))

// Check that the Windows version is RS4 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS4_OR_LATER (NTDDI_WIN10_RS4 && (NTDDI_VERSION >= NTDDI_WIN10_RS4))

// Check that the Windows version is RS5 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS5_OR_LATER (NTDDI_WIN10_RS5 && (NTDDI_VERSION >= NTDDI_WIN10_RS5))

// Check that the Windows version is 19H1 or earlier. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_19H1_OR_EARLIER (!(NTDDI_WIN10_19H1 && (NTDDI_VERSION > NTDDI_WIN10_19H1)))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// All include files needed by all Modules and the Framework.
// This ensures that all Modules always compile together so that any Module can always be used with any other
// Module without having to deal with include file dependencies.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Some environments use DBG instead of DEBUG. DMF uses DEBUG so, define DEBUG in that case.
//
#if DBG
    #if !defined(DEBUG)
        #define DEBUG
    #endif
#endif

// All DMF Modules need these.
//
#include <stdlib.h>
#include <sal.h>
#if defined(DMF_USER_MODE)
    #include <windows.h>
    #include <stdio.h>
    #if !defined(DMF_WIN32_MODE)
        #include <wdf.h>
    #endif
    #include <Objbase.h>
    // NOTE: This file includes poclass.h. Do not include that again
    //       otherwise, redefinition errors will occur.
    //
    #include <batclass.h>
    #include <hidclass.h>
    #include <powrprof.h>
    #include <usbiodef.h>
    #include <cguid.h>
    #include <guiddef.h>
    #include <wdmguid.h>
    #include <cfgmgr32.h>
    #include <ndisguid.h>
    #include <strsafe.h>
    #include <ndisguid.h>
    // TODO: Add support for USB in User Mode drivers.
    //
    #if !defined(DMF_WIN32_MODE)
        // Turn this on to debug asserts in UMDF.
        // Normal assert() causes a crash in UMDF which causes UMDF to just disable the driver
        // without showing what assert failed.
        //
        #if defined(DEBUG)
            #if defined(USE_ASSERT_BREAK)
                // It means, check a condition...If it is false, break into debugger.
                //
                #pragma warning(disable:4127)
                #if defined(ASSERT)
                    #undef ASSERT
                #endif // defined(ASSERT)
                #define ASSERT(X)   ((! (X)) ? DebugBreak() : TRUE)
            #else
                #if !defined(ASSERT)
                    // It means, use native assert().
                    //
                    #include <assert.h>
                    #define ASSERT(X)   assert(X)
                #endif // !defined(ASSERT)
            #endif // defined(USE_ASSERT_BREAK)
        #else
            #if !defined(ASSERT)
                // It means, do not assert at all.
                //
                #define ASSERT(X) TRUE
            #endif // !defined(ASSERT)
        #endif // defined(DEBUG)
    #else
        #if defined(DEBUG)
            #if defined(USE_ASSERT_BREAK)
                // It means, check a condition...If it is false, break into debugger.
                //
                #pragma warning(disable:4127)
                #if defined(ASSERT)
                    #undef ASSERT
                #endif // defined(ASSERT)
                #define ASSERT(X)   ((! (X)) ? DebugBreak() : TRUE)
            #else
                #if !defined(ASSERT)
                    // It means, use native assert().
                    //
                    #include <assert.h>
                    #define ASSERT(X)   assert(X)
                #endif // !defined(ASSERT)
            #endif // defined(USE_ASSERT_BREAK)
        #else
            #if !defined(ASSERT)
                // It means, do not assert at all.
                //
                #define ASSERT(X) TRUE
            #endif // !defined(ASSERT)
        #endif // defined(DEBUG)
    #endif
#else
    // Kernel-mode
    //
    #include <ntifs.h>
    #include <wdm.h>
    #include <ntddk.h>
    #include <ntstatus.h>
    #include <ntintsafe.h>
    // TODO: Add this after Dmf_Registry supports this definition properly.
    // #define NTSTRSAFE_LIB
    //
    #include <ntstrsafe.h>
    #include <wdf.h>
    // NOTE: This file has be listed here. Listing it later causes many redefinition compilation errors.
    //
    #include <acpiioct.h>
    #include <wmiguid.h>
    #include <ntddstor.h>
    #include <stdarg.h>
    #include <ntddser.h>
    #include <guiddef.h>
    #include <wdmguid.h>
    #include <ntddvdeo.h>
    #include <spb.h>
    // NOTE: This file includes poclass.h. Do not include that again
    //       otherwise, redefinition errors will occur.
    //
    #include <batclass.h>
    #include <hidport.h>
    #include "usbdi.h"
    #include "usbdlib.h"
    #include <wdfusb.h>
    #include <wpprecorder.h>
    #if IS_WIN10_RS3_OR_LATER
        #include <lkmdtel.h>
    #endif // IS_WIN10_RS3_OR_LATER
    #include <intrin.h>
#endif // defined(DMF_USER_MODE)

// DMF Asserts definitions 
//
#if defined(DMF_USER_MODE) && !defined(DMF_WIN32_MODE)
    #if DBG
        #if defined(NO_USE_ASSERT_BREAK)
            #include <assert.h>
            #define DmfAssertMessage(Message, Expression) (!(Expression) ? assert(Expression), FALSE : TRUE)
        #else
            #define DmfAssertMessage(Message, Expression) (!(Expression) ? DbgBreakPoint(), OutputDebugStringA(Message), FALSE : TRUE)
        #endif
    #else
        #define DmfAssertMessage(Message, Expression) TRUE        
    #endif
    #define DmfVerifierAssert(Message, Expression)                          \
        if ((WdfDriverGlobals->DriverFlags & WdfVerifyOn) && !(Expression)) \
        {                                                                   \
            OutputDebugStringA(Message);                                    \
            DbgBreakPoint();                                                \
        }
#elif defined(DMF_WIN32_MODE)
    #if DBG
        #if defined(NO_USE_ASSERT_BREAK)
            #include <assert.h>
            #define DmfAssertMessage(Message, Expression) (!(Expression) ? assert(Expression), FALSE : TRUE)
        #else
            #define DmfAssertMessage(Message, Expression) (!(Expression) ? DebugBreak(), printf(Message), FALSE : TRUE)
        #endif
    #else
        #define DmfAssertMessage(Message, Expression) TRUE        
    #endif
    #define DmfVerifierAssert(Message, Expression)                          \
        if (!(Expression))                                                  \
        {                                                                   \
            printf(Message);                                                \
            DebugBreak();                                                   \
        }
#else
    #define DmfAssertMessage(Message, Expression) ASSERTMSG(Message, Expression)
    #define DmfVerifierAssert(Message, Expression)                          \
        if ((WdfDriverGlobals->DriverFlags & WdfVerifyOn) && !(Expression)) \
        {                                                                   \
            RtlAssert( Message, __FILE__, __LINE__, NULL );                 \
        }
#endif

#define DmfAssert(Expression) DmfAssertMessage(#Expression, Expression)

//////////////////////////////////////////////////////////////////////////////////////
// Definitions for all non-native platforms.
//////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_WIN32_MODE) || defined(DMF_XXX_MODE)

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

    // TODO: From WDM.
    //
    #define DEVICE_TYPE DWORD

    #define WDF_EVERYTHING_ALWAYS_AVAILABLE
    #include ".\Platform\wudfwdm.h"
    #include ".\Platform\wdftypes.h"
    #include ".\Platform\wdfglobals.h"
    #include ".\Platform\wdffuncenum.h"

    typedef VOID (*WDFFUNC) (VOID);
    extern WDFFUNC WdfFunctions[WdfFunctionTableNumEntries];

    #include ".\Platform\wdfobject.h"
    #include ".\Platform\wdfcore.h"

    // TODO: From WDM.
    //
    typedef struct _PNP_BUS_INFORMATION {
        GUID BusTypeGuid;
        INTERFACE_TYPE LegacyBusType;
        ULONG BusNumber;
    } PNP_BUS_INFORMATION, *PPNP_BUS_INFORMATION;
    #define WDF_DEVICE_NO_WDMSEC_H
    #include ".\Platform\wdfdevice.h"

    #include ".\Platform\wdfmemory.h"
    #include ".\Platform\wdfsync.h"
    #include ".\Platform\wdftimer.h"
    #include ".\Platform\wdfworkitem.h"
    #include ".\Platform\wdfcollection.h"

    // TODO: From WDM.
    //
    typedef struct 
    {
        ULONG Dummy;
    }IO_STACK_LOCATION, *PIO_STACK_LOCATION;
    #include ".\Platform\wdfrequest.h"
    #include ".\Platform\wdfio.h"
    #include ".\Platform\wdffileobject.h"

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

    // Forward declaration for DMF Platform Object.
    //
    typedef struct _DMF_PLATFORM_OBJECT_ DMF_PLATFORM_OBJECT;

    // Delete callback for each type of data.
    //
    typedef void (*DMF_Platform_ObjectDelete)(_In_ DMF_PLATFORM_OBJECT* PlatformObject);

    // NOTE: This is analogous to WDFOBJECT.
    //
    struct _DMF_PLATFORM_OBJECT_
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
        LONG NumberOfChildren;
        // TODO: Need this definition to go above as DMF_PLATFORM_CRITICAL_SECTION.
        //
        CRITICAL_SECTION ChildListLock;
        // Allows this structure to be inserted into the parent's list
        // of child objects so that the tree of objects can be deleted
        // automatically when the parent is deleted.
        //
        LIST_ENTRY ChildListEntry;
        // Maintains a list of contexts.
        //
        LIST_ENTRY ContextList;
        CRITICAL_SECTION ContextListLock;
        // Reference count.
        //
        LONG ReferenceCount;
        // Deletion callback for custom object data.
        //
        DMF_Platform_ObjectDelete ObjectDelete;
    };

    typedef struct
    {
        LIST_ENTRY ListEntry;
        WDF_OBJECT_CONTEXT_TYPE_INFO ContextTypeInfo;
        VOID* ContextData;
    } DMF_PLATFORM_CONTEXT;

    // NOTE: Every platform defines platform specific versions of these
    //       structures.
    //
    struct _DMF_PLATFORM_MEMORY;
    struct _DMF_PLATFORM_SPINLOCK;
    struct _DMF_PLATFORM_WAITLOCK;
    struct _DMF_PLATFORM_TIMER;
    struct _DMF_PLATFORM_WORKITEM;
    struct _DMF_PLATFORM_COLLECTION;
    struct _DMF_PLATFORM_QUEUE;
    struct _DMF_PLATFORM_DEVICE;

    // NOTE: Every platform defines platform specific versions of these
    //       functions.
    //

    // TODO: We should return possible return a context that is used later
    //       so platform can store platform specific data. (Use global memory
    //       for that for now.)
    //
    typedef
    void
    (*DmfPlatformHandler_PlatformInitialize)(
        void
        );

    typedef
    void
    (*DmfPlatformHandler_PlatformUninitialize)(
        void
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfTimerCreate)(
        _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
        _In_ DMF_PLATFORM_OBJECT* PlatformObject
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfTimerStart)(
        _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
        _In_ LONGLONG DueTime
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfTimerStop)(
        _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
        _In_ BOOLEAN Wait
        );

    typedef
    void
    (*DmfPlatformHandler_WdfTimerDelete)(
        _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfWorkItemCreate)(
        _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem,
        _In_ DMF_PLATFORM_OBJECT* PlatformObject
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfWorkItemEnqueue)(
        _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
        );

    typedef
    VOID
    (*DmfPlatformHandler_WdfWorkItemFlush)(
        _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
        );

    typedef
    void
    (*DmfPlatformHandler_WdfWorkItemDelete)(
        _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfWaitLockCreate)(
        _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
        );

    typedef
    DWORD
    (*DmfPlatformHandler_WdfWaitLockAcquire)(
        _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock,
        _In_ DWORD TimeoutMs
        );

    typedef
    void
    (*DmfPlatformHandler_WdfWaitLockRelease)(
        _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
        );

    typedef
    void
    (*DmfPlatformHandler_WdfWaitLockDelete)(
        _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
        );

    typedef
    BOOLEAN
    (*DmfPlatformHandler_WdfSpinLockCreate)(
        _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
        );

    typedef
    void
    (*DmfPlatformHandler_WdfSpinLockAcquire)(
        _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
        );

    typedef
    void
    (*DmfPlatformHandler_WdfSpinLockRelease)(
        _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
        );

    typedef
    void
    (*DmfPlatformHandler_WdfSpinLockDelete)(
        _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
        );

    // Structure of all the mapped handlers.
    //
    typedef struct
    {
        DmfPlatformHandler_PlatformInitialize DmfHandlerPlatformInitialize;
        DmfPlatformHandler_PlatformUninitialize DmfHandlerPlatformUninitialize;
        DmfPlatformHandler_WdfTimerCreate DmfHandlerWdfTimerCreate;
        DmfPlatformHandler_WdfTimerStart DmfHandlerWdfTimerStart;
        DmfPlatformHandler_WdfTimerStop DmfHandlerWdfTimerStop;
        DmfPlatformHandler_WdfTimerDelete DmfHandlerWdfTimerDelete;
        DmfPlatformHandler_WdfWorkItemCreate DmfHandlerWdfWorkItemCreate;
        DmfPlatformHandler_WdfWorkItemEnqueue DmfHandlerWdfWorkItemEnqueue;
        DmfPlatformHandler_WdfWorkItemFlush DmfHandlerWdfWorkItemFlush;
        DmfPlatformHandler_WdfWorkItemDelete DmfHandlerWdfWorkItemDelete;
        DmfPlatformHandler_WdfWaitLockCreate DmfHandlerWdfWaitLockCreate;
        DmfPlatformHandler_WdfWaitLockAcquire DmfHandlerWdfWaitLockAcquire;
        DmfPlatformHandler_WdfWaitLockRelease DmfHandlerWdfWaitLockRelease;
        DmfPlatformHandler_WdfWaitLockDelete DmfHandlerWdfWaitLockDelete;
        DmfPlatformHandler_WdfSpinLockCreate DmfHandlerWdfSpinLockCreate;
        DmfPlatformHandler_WdfSpinLockAcquire DmfHandlerWdfSpinLockAcquire;
        DmfPlatformHandler_WdfSpinLockRelease DmfHandlerWdfSpinLockRelease;
        DmfPlatformHandler_WdfSpinLockDelete DmfHandlerWdfSpinLockDelete;
    } DmfPlatform_Handlers;

#endif

//////////////////////////////////////////////////////////////////////////////////////
// Definitions for specific platforms.
//////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_WIN32_MODE)
    #include "Platform\DmfPlatform_Win32.h"
#elif defined(DMF_XXX_MODE)
    // PLATFORM_TEMPLATE:
    //
#else
    // Use native support.
    //
#endif

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// These probably need to be deleted for non-Windows platforms.
//
#include <hidusage.h>
#include <hidpi.h>

// NOTE: This is necessary in order to avoid redefinition errors. It is not clear why
//       this is the case.
//
#define DEVPKEY_H_INCLUDED

// eof: DmfPlatform.h
//
