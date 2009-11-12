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


// This file declares the CameraInfo class.

#ifndef O3D_IMPORT_CROSS_CAMERA_INFO_H_
#define O3D_IMPORT_CROSS_CAMERA_INFO_H_

#include "import/cross/json_object.h"

namespace o3d {

// CameraInfo is a used for serialization only and is not part of the
// normal O3D plugin. It is used for information about the camera.
class CameraInfo : public JSONObject {
 public:
  typedef SmartPointer<CameraInfo> Ref;

  static const char* kAspectRatioParamName;
  static const char* kNearZParamName;
  static const char* kFarZParamName;
  static const char* kTransformValueName;
  static const char* kEyeValueName;
  static const char* kTargetValueName;
  static const char* kUpValueName;

  // Gets the near_z.
  float near_z() const {
    return near_z_param_->value();
  }

  // Sets the near_z.
  void set_near_z(float value) {
    near_z_param_->set_value(value);
  }

  // Gets the far_z.
  float far_z() const {
    return far_z_param_->value();
  }

  // Sets the far_z.
  void set_far_z(float value) {
    far_z_param_->set_value(value);
  }

  // Gets the aspect_ratio.
  float aspect_ratio() const {
    return aspect_ratio_param_->value();
  }

  // Sets the aspect_ratio.
  void set_aspect_ratio(float value) {
    aspect_ratio_param_->set_value(value);
  }

  // Gets the transform.
  Transform* transform() const {
    return transform_value_->value();
  }

  // Sets the transform.
  void set_transform(Transform* value) {
    transform_value_->set_value(value);
  }

  // Gets the eye.
  Float3 eye() const {
    return eye_value_->value();
  }

  // Sets the eye.
  void set_eye(const Float3& value) {
    eye_value_->set_value(value);
  }

  // Gets the target.
  Float3 target() const {
    return target_value_->value();
  }

  // Sets the target.
  void set_target(const Float3& value) {
    target_value_->set_value(value);
  }

  // Gets the up.
  Float3 up() const {
    return up_value_->value();
  }

  // Sets the up.
  void set_up(const Float3& value) {
    up_value_->set_value(value);
  }

 protected:
  explicit CameraInfo(ServiceLocator* service_locator);

 private:
  // We make these params so we can easily attach animation to the camera's
  // parameters.
  ParamFloat::Ref aspect_ratio_param_;
  ParamFloat::Ref near_z_param_;
  ParamFloat::Ref far_z_param_;

  // These are not params, just JSON values.
  JSONTransform::Ref transform_value_;
  JSONOptionalFloat3::Ref eye_value_;
  JSONOptionalFloat3::Ref target_value_;
  JSONOptionalFloat3::Ref up_value_;

  O3D_OBJECT_BASE_DECL_CLASS(CameraInfo, JSONObject);
  DISALLOW_COPY_AND_ASSIGN(CameraInfo);
};

class OrthographicCameraInfo : public CameraInfo {
 public:
  typedef SmartPointer<OrthographicCameraInfo> Ref;

  static const char* kMagXParamName;
  static const char* kMagYParamName;

  // Gets the mag_x.
  float mag_x() const {
    return mag_x_param_->value();
  }

  // Sets the mag_x.
  void set_mag_x(float value) {
    mag_x_param_->set_value(value);
  }

  // Gets the mag_y.
  float mag_y() const {
    return mag_y_param_->value();
  }

  // Sets the mag_y.
  void set_mag_y(float value) {
    mag_y_param_->set_value(value);
  }

 private:
  explicit OrthographicCameraInfo(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // We make these params so we can easily attach animation to the camera's
  // parameters.
  ParamFloat::Ref mag_x_param_;
  ParamFloat::Ref mag_y_param_;

  O3D_OBJECT_BASE_DECL_CLASS(OrthographicCameraInfo, CameraInfo);
  DISALLOW_COPY_AND_ASSIGN(OrthographicCameraInfo);
};

class PerspectiveCameraInfo : public CameraInfo {
 public:
  typedef SmartPointer<PerspectiveCameraInfo> Ref;

  static const char* kFieldOfViewYParamName;

  // Gets the field_of_view_y.
  float field_of_view_y() const {
    return field_of_view_y_param_->value();
  }

  // Sets the field_of_view_y.
  void set_field_of_view_y(float value) {
    field_of_view_y_param_->set_value(value);
  }

 private:
  explicit PerspectiveCameraInfo(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // We make these params so we can easily attach animation to the camera's
  // parameters.
  ParamFloat::Ref field_of_view_y_param_;

  O3D_OBJECT_BASE_DECL_CLASS(PerspectiveCameraInfo, CameraInfo);
  DISALLOW_COPY_AND_ASSIGN(PerspectiveCameraInfo);
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_CAMERA_INFO_H_

