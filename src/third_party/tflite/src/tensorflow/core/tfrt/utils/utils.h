/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_CORE_TFRT_UTILS_H_
#define TENSORFLOW_CORE_TFRT_UTILS_H_

#include <string>

#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/tfrt/runtime/runtime.h"
#include "tensorflow/core/tfrt/utils/statusor.h"
#include "tfrt/bef/bef_buffer.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime
#include "tfrt/support/forward_decls.h"  // from @tf_runtime

namespace tensorflow {
class Device;
class EagerContext;
}  // namespace tensorflow

namespace tfrt {

class BEFFile;
class ExecutionContext;
class HostContext;

typedef tensorflow::gtl::InlinedVector<tfrt::DType, 4> TfrtDataTypeVector;
typedef tensorflow::gtl::ArraySlice<tfrt::DType> TfrtDataTypeSlice;

// TODO(b/161370736): Have a formal method to convert between TF's and TFRT's
// device name. Currently TFRT adopts the suffix of TF's device name,
// e.g. CPU:0.
Expected<const char*> ConvertTfDeviceNameToTfrt(
    const char* device_name, tensorflow::EagerContext* eager_context);

DType ConvertTfDTypeToTfrtDType(tensorflow::DataType dtype);

// Runs the runtime initialization function. A runtime initialization function
// is added by runtime/compiler workflow and is not present in the original
// savedmodel.
//
// TODO(b/178714905): We should avoid special handling on initialization by
// letting compiler to handle it.
tensorflow::Status RunRuntimeInitializer(const tfrt::ExecutionContext& exec_ctx,
                                         tfrt::BEFFile* bef_file,
                                         absl::string_view fallback_init_func);

// Creates dummy TF devices from the input device names. Currently this method
// is used to create the TPU_SYSTEM device for worker server.
void CreateDummyTfDevices(
    const std::vector<std::string>& device_names,
    std::vector<std::unique_ptr<tensorflow::Device>>* dummy_tf_devices);

// Creates and add dummy TFRT devices from the input device names. Currently
// this method is used to create the TPU_SYSTEM device for worker server.
void AddDummyTfrtDevices(const std::vector<std::string>& device_names,
                         tfrt::HostContext* host_ctx);

// Creates a BEF file from a BEF buffer. `runtime` is used to provide host
// context for opening `bef`.
StatusOr<RCReference<tfrt::BEFFile>> CreateBefFileFromBefBuffer(
    const tensorflow::tfrt_stub::Runtime& runtime, const tfrt::BefBuffer& bef);

}  // namespace tfrt

#endif  // TENSORFLOW_CORE_TFRT_UTILS_H_
