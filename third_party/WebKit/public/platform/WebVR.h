// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebVR_h
#define WebVR_h

#include "WebCommon.h"

#if BLINK_IMPLEMENTATION
#include "wtf/Assertions.h"
#endif

namespace blink {

struct WebVRVector3 {
    float x, y, z;
};

struct WebVRVector4 {
    float x, y, z, w;
};

// A field of view, given by 4 degrees describing the view from a center point.
struct WebVRFieldOfView {
    float upDegrees;
    float downDegrees;
    float leftDegrees;
    float rightDegrees;
};

// Bit flags to indicate which fields of an WebHMDSensorState are valid.
enum WebVRSensorStateFlags {
    WebVRSensorStateOrientation = 1 << 1,
    WebVRSensorStatePosition = 1 << 2,
    WebVRSensorStateAngularVelocity = 1 << 3,
    WebVRSensorStateLinearVelocity = 1 << 4,
    WebVRSensorStateAngularAcceleration = 1 << 5,
    WebVRSensorStateLinearAcceleration = 1 << 6,
    WebVRSensorStateComplete = (1 << 7) - 1 // All previous states combined.
};

// A bitfield of WebVRSensorStateFlags.
typedef int WebVRSensorStateMask;

// A sensor's position, orientation, velocity, and acceleration state at the
// given timestamp.
struct WebHMDSensorState {
    double timestamp;
    unsigned frameIndex;
    WebVRSensorStateMask flags;
    WebVRVector4 orientation;
    WebVRVector3 position;
    WebVRVector3 angularVelocity;
    WebVRVector3 linearVelocity;
    WebVRVector3 angularAcceleration;
    WebVRVector3 linearAcceleration;
};

// Information about the optical properties for an eye in an HMD.
struct WebVREyeParameters {
    WebVRFieldOfView minimumFieldOfView;
    WebVRFieldOfView maximumFieldOfView;
    WebVRFieldOfView recommendedFieldOfView;
    WebVRVector3 eyeTranslation;
};

// Information pertaining to Head Mounted Displays.
struct WebVRHMDInfo {
    WebVREyeParameters leftEye;
    WebVREyeParameters rightEye;
};

// Bit flags to indicate what type of data a WebVRDevice describes.
enum WebVRDeviceTypeFlags {
    WebVRDeviceTypePosition = 1 << 1,
    WebVRDeviceTypeHMD = 1 << 2
};

// A bitfield of WebVRDeviceTypeFlags.
typedef int WebVRDeviceTypeMask;

// Describes a single VR hardware unit. May describe multiple capabilities,
// such as position sensors or head mounted display metrics.
class WebVRDevice {
public:
    static const size_t deviceIdLengthCap = 128;
    static const size_t deviceNameLengthCap = 128;

    WebVRDevice()
        : flags(0)
    {
        deviceId[0] = 0;
        deviceName[0] = 0;
    }

    // Index for this hardware unit.
    unsigned index;
    // Device identifier (based on manufacturer, model, etc.).
    WebUChar deviceId[deviceIdLengthCap];
    // Friendly device name.
    WebUChar deviceName[deviceNameLengthCap];
    // Identifies the capabilities of this hardware unit.
    WebVRDeviceTypeMask flags;

    // Will only contain valid data if (flags & HasHMDDevice).
    WebVRHMDInfo hmdInfo;
};

}

#endif // WebVR_h
