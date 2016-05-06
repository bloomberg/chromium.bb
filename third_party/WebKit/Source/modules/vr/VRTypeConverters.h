// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRTypeConverters_h
#define VRTypeConverters_h

#include "mojo/public/cpp/bindings/type_converter.h"
#include "public/platform/modules/vr/WebVR.h"
#include "public/platform/modules/vr/vr_service.mojom.h"

namespace mojo {

// Type/enum conversions from WebVR data types to Mojo data types
// and vice versa.

template <>
struct TypeConverter<blink::WebVRVector3, blink::mojom::VRVector3Ptr> {
    static blink::WebVRVector3 Convert(const blink::mojom::VRVector3Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRVector4, blink::mojom::VRVector4Ptr> {
    static blink::WebVRVector4 Convert(const blink::mojom::VRVector4Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRRect, blink::mojom::VRRectPtr> {
    static blink::WebVRRect Convert(const blink::mojom::VRRectPtr& input);
};

template <>
struct TypeConverter<blink::WebVRFieldOfView, blink::mojom::VRFieldOfViewPtr> {
    static blink::WebVRFieldOfView Convert(
        const blink::mojom::VRFieldOfViewPtr& input);
};

template <>
struct TypeConverter<blink::WebVREyeParameters, blink::mojom::VREyeParametersPtr> {
    static blink::WebVREyeParameters Convert(
        const blink::mojom::VREyeParametersPtr& input);
};

template <>
struct TypeConverter<blink::WebVRHMDInfo, blink::mojom::VRHMDInfoPtr> {
    static blink::WebVRHMDInfo Convert(const blink::mojom::VRHMDInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebVRDevice, blink::mojom::VRDeviceInfoPtr> {
    static blink::WebVRDevice Convert(const blink::mojom::VRDeviceInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebHMDSensorState, blink::mojom::VRSensorStatePtr> {
    static blink::WebHMDSensorState Convert(
        const blink::mojom::VRSensorStatePtr& input);
};

} // namespace mojo

#endif // VRTypeConverters_h
