#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_HingeAngle.h

Abstract:

    Companion file to Dmf_HingeAngle.cpp.

Environment:

    User-mode Driver Framework

--*/

// This Module uses C++/WinRT so it needs RS5+ support. 
// This code will not be compiled in RS4 and below.
//
#if defined(DMF_USER_MODE) && IS_WIN10_RS5_OR_LATER && defined(__cplusplus)

#include "winrt/Windows.Devices.Sensors.h"

typedef struct _HINGE_ANGLE_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    double AngleInDegrees;
} HINGE_ANGLE_SENSOR_STATE;

// DMF Module event callback.
//
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ HINGE_ANGLE_SENSOR_STATE* HingeAngleSensorState
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific hinge angle device Id to open. This is optional.
    //
    winrt::hstring DeviceId;
    // Report threshold in degrees.
    //
    double ReportThresholdInDegrees;
    // Callback to inform Parent Module that hinge angle has new changed reading.
    //
    EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback* EvtHingeAngleReadingChangeCallback;
} DMF_CONFIG_HingeAngle;

// This macro declares the following functions:
// DMF_HingeAngle_ATTRIBUTES_INIT()
// DMF_CONFIG_HingeAngle_AND_ATTRIBUTES_INIT()
// DMF_HingeAngle_Create()
//
DECLARE_DMF_MODULE(HingeAngle)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HingeAngle_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ HINGE_ANGLE_SENSOR_STATE* CurrentState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HingeAngle_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HingeAngle_Stop(
    _In_ DMFMODULE DmfModule
    );

#endif // defined(DMF_USER_MODE) && defined(IS_WIN10_RS5_OR_LATER) && defined(__cplusplus)

// eof: Dmf_HingeAngle.h
//
