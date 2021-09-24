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

#include "tensorflow/compiler/xla/service/gpu/nccl_test_utils.h"

#include <memory>

#include "tensorflow/compiler/xla/service/gpu/nccl_utils.h"

namespace xla {
namespace gpu {

absl::flat_hash_set<GlobalDeviceId> DevicesWithOpenNcclChannels() {
  absl::flat_hash_set<GlobalDeviceId> devices;
  NcclCliqueCache().ForEach([&](const NcclCliqueKey& k, const NcclClique&) {
    devices.insert(k.devices().begin(), k.devices().end());
  });
  return devices;
}

}  // namespace gpu
}  // namespace xla
