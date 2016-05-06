// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRTypeConverters.h"

#include <string.h>
#include <algorithm>

using blink::mojom::VRVector3Ptr;
using blink::mojom::VRVector4Ptr;
using blink::mojom::VRRectPtr;
using blink::mojom::VRFieldOfViewPtr;
using blink::mojom::VREyeParametersPtr;
using blink::mojom::VRHMDInfoPtr;
using blink::mojom::VRDeviceInfoPtr;
using blink::mojom::VRSensorStatePtr;

namespace mojo {

// static
blink::WebVRVector3 TypeConverter<blink::WebVRVector3, VRVector3Ptr>::Convert(
    const VRVector3Ptr& input)
{
    blink::WebVRVector3 output;
    output.x = input->x;
    output.y = input->y;
    output.z = input->z;
    return output;
}

// static
blink::WebVRVector4 TypeConverter<blink::WebVRVector4, VRVector4Ptr>::Convert(
    const VRVector4Ptr& input)
{
    blink::WebVRVector4 output;
    output.x = input->x;
    output.y = input->y;
    output.z = input->z;
    output.w = input->w;
    return output;
}

// static
blink::WebVRRect TypeConverter<blink::WebVRRect, VRRectPtr>::Convert(
    const VRRectPtr& input)
{
    blink::WebVRRect output;
    output.x = input->x;
    output.y = input->y;
    output.width = input->width;
    output.height = input->height;
    return output;
}

// static
blink::WebVRFieldOfView
TypeConverter<blink::WebVRFieldOfView, VRFieldOfViewPtr>::Convert(
    const VRFieldOfViewPtr& input)
{
    blink::WebVRFieldOfView output;
    output.upDegrees = input->upDegrees;
    output.downDegrees = input->downDegrees;
    output.leftDegrees = input->leftDegrees;
    output.rightDegrees = input->rightDegrees;
    return output;
}

// static
blink::WebVREyeParameters
TypeConverter<blink::WebVREyeParameters, VREyeParametersPtr>::Convert(
    const VREyeParametersPtr& input)
{
    blink::WebVREyeParameters output;
    output.minimumFieldOfView = input->minimumFieldOfView.To<blink::WebVRFieldOfView>();
    output.maximumFieldOfView = input->maximumFieldOfView.To<blink::WebVRFieldOfView>();
    output.recommendedFieldOfView = input->recommendedFieldOfView.To<blink::WebVRFieldOfView>();
    output.eyeTranslation = input->eyeTranslation.To<blink::WebVRVector3>();
    output.renderRect = input->renderRect.To<blink::WebVRRect>();
    return output;
}

// static
blink::WebVRHMDInfo TypeConverter<blink::WebVRHMDInfo, VRHMDInfoPtr>::Convert(
    const VRHMDInfoPtr& input)
{
    blink::WebVRHMDInfo output;
    output.leftEye = input->leftEye.To<blink::WebVREyeParameters>();
    output.rightEye = input->rightEye.To<blink::WebVREyeParameters>();
    return output;
}

// static
blink::WebVRDevice TypeConverter<blink::WebVRDevice, VRDeviceInfoPtr>::Convert(
    const VRDeviceInfoPtr& input)
{
    blink::WebVRDevice output;
    memset(&output, 0, sizeof(blink::WebVRDevice));

    output.index = input->index;
    output.flags = blink::WebVRDeviceTypePosition;
    output.deviceName = blink::WebString::fromUTF8(input->deviceName.data(),
        input->deviceName.size());

    if (!input->hmdInfo.is_null()) {
        output.flags |= blink::WebVRDeviceTypeHMD;
        output.hmdInfo = input->hmdInfo.To<blink::WebVRHMDInfo>();
    }

    return output;
}

// static
blink::WebHMDSensorState
TypeConverter<blink::WebHMDSensorState, VRSensorStatePtr>::Convert(
    const VRSensorStatePtr& input)
{
    blink::WebHMDSensorState output;
    output.timestamp = input->timestamp;
    output.frameIndex = input->frameIndex;
    output.flags = 0;

    if (!input->orientation.is_null()) {
        output.flags |= blink::WebVRSensorStateOrientation;
        output.orientation = input->orientation.To<blink::WebVRVector4>();
    }
    if (!input->position.is_null()) {
        output.flags |= blink::WebVRSensorStatePosition;
        output.position = input->position.To<blink::WebVRVector3>();
    }
    if (!input->angularVelocity.is_null()) {
        output.flags |= blink::WebVRSensorStateAngularVelocity;
        output.angularVelocity = input->angularVelocity.To<blink::WebVRVector3>();
    }
    if (!input->linearVelocity.is_null()) {
        output.flags |= blink::WebVRSensorStateLinearVelocity;
        output.linearVelocity = input->linearVelocity.To<blink::WebVRVector3>();
    }
    if (!input->angularAcceleration.is_null()) {
        output.flags |= blink::WebVRSensorStateAngularAcceleration;
        output.angularAcceleration = input->angularAcceleration.To<blink::WebVRVector3>();
    }
    if (!input->linearAcceleration.is_null()) {
        output.flags |= blink::WebVRSensorStateLinearAcceleration;
        output.linearAcceleration = input->linearAcceleration.To<blink::WebVRVector3>();
    }

    return output;
}

} // namespace mojo
