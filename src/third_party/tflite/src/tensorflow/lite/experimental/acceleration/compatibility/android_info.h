/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_COMPATIBILITY_ANDROID_INFO_H_
#define TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_COMPATIBILITY_ANDROID_INFO_H_

#include <string>

#include "absl/status/status.h"

namespace tflite {
namespace acceleration {

// Information about and Android device, used for determining compatibility
// status.
struct AndroidInfo {
  // Property ro.build.version.sdk
  std::string android_sdk_version;
  // Property ro.product.model
  std::string model;
  // Property ro.product.device
  std::string device;
  // Property ro.product.manufacturer
  std::string manufacturer;
};

absl::Status RequestAndroidInfo(AndroidInfo* info_out);

}  // namespace acceleration
}  // namespace tflite

#endif  // TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_COMPATIBILITY_ANDROID_INFO_H_
