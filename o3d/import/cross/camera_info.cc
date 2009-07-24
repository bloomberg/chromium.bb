/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "import/cross/camera_info.h"

namespace o3d {

O3D_OBJECT_BASE_DEFN_CLASS(
    "o3djs.CameraInfo", CameraInfo, JSONObject);
O3D_OBJECT_BASE_DEFN_CLASS(
    "o3djs.PerspectiveCameraInfo", PerspectiveCameraInfo, CameraInfo);
O3D_OBJECT_BASE_DEFN_CLASS(
    "o3djs.OrthographicCameraInfo", OrthographicCameraInfo, CameraInfo);

const char* CameraInfo::kAspectRatioParamName = "aspectRatio";
const char* CameraInfo::kNearZParamName = "nearZ";
const char* CameraInfo::kFarZParamName = "farZ";
const char* CameraInfo::kTransformValueName = "transform";
const char* CameraInfo::kEyeValueName = "eye";
const char* CameraInfo::kTargetValueName = "target";
const char* CameraInfo::kUpValueName = "up";

CameraInfo::CameraInfo(ServiceLocator* service_locator)
    : JSONObject(service_locator) {
  RegisterParamRef(kAspectRatioParamName, &aspect_ratio_param_);
  RegisterParamRef(kNearZParamName, &near_z_param_);
  RegisterParamRef(kFarZParamName, &far_z_param_);
  RegisterJSONValue(kTransformValueName, &transform_value_);
  RegisterJSONValue(kEyeValueName, &eye_value_);
  RegisterJSONValue(kTargetValueName, &target_value_);
  RegisterJSONValue(kUpValueName, &up_value_);

  set_aspect_ratio(1.0f);
  set_near_z(0.01f);
  set_far_z(10000.0f);
}

const char* OrthographicCameraInfo::kMagXParamName = "magX";
const char* OrthographicCameraInfo::kMagYParamName = "magY";

OrthographicCameraInfo::OrthographicCameraInfo(ServiceLocator* service_locator)
    : CameraInfo(service_locator) {
  RegisterParamRef(kMagXParamName, &mag_x_param_);
  RegisterParamRef(kMagYParamName, &mag_y_param_);

  set_mag_x(1.0f);
  set_mag_y(1.0f);
}

ObjectBase::Ref OrthographicCameraInfo::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new OrthographicCameraInfo(service_locator));
}

const char* PerspectiveCameraInfo::kFieldOfViewYParamName = "fieldOfViewY";

PerspectiveCameraInfo::PerspectiveCameraInfo(ServiceLocator* service_locator)
    : CameraInfo(service_locator) {
  RegisterParamRef(kFieldOfViewYParamName, &field_of_view_y_param_);

  set_field_of_view_y(30.0f * 3.14159f / 180.0f);
}

ObjectBase::Ref PerspectiveCameraInfo::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new PerspectiveCameraInfo(service_locator));
}

}  // namespace o3d


