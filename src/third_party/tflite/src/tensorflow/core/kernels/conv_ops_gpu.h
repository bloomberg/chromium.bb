/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_KERNELS_CONV_OPS_GPU_H_
#define TENSORFLOW_CORE_KERNELS_CONV_OPS_GPU_H_

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#include <tuple>
#include <unordered_map>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/kernels/gpu_utils.h"
#include "tensorflow/core/lib/gtl/inlined_vector.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/util/tensor_format.h"

namespace tensorflow {

// Get the Dnn workspace limit from the environment variable, which is in MB.
// Return the workspace memory limit in bytes. If no value is set, return the
// default value.
int64 GetDnnWorkspaceLimit(const string& envvar_in_mb,
                           int64_t default_value_in_bytes);

// A class to provide scratch-space allocator for Stream-Executor Cudnn
// callback. TensorFlow is responsible for releasing the temporary buffers after
// the kernel finishes.
class DnnScratchAllocator : public se::ScratchAllocator {
 public:
  virtual ~DnnScratchAllocator() {}
  DnnScratchAllocator(int64_t memory_limit, OpKernelContext* context)
      : memory_limit_(memory_limit), total_byte_size_(0), context_(context) {}
  int64 GetMemoryLimitInBytes() override { return memory_limit_; }
  se::port::StatusOr<se::DeviceMemory<uint8>> AllocateBytes(
      int64_t byte_size) override {
    Tensor temporary_memory;
    if (byte_size < 0) {
      return se::port::Status{se::port::error::INVALID_ARGUMENT,
                              "Requested negative byte size!"};
    }
    if (byte_size > memory_limit_) {
      return se::port::Status{se::port::error::UNAVAILABLE,
                              absl::StrCat("Requested memory size (", byte_size,
                                           ") exceeds the max memory limit (",
                                           memory_limit_, ").")};
    }
    AllocationAttributes allocation_attr;
    allocation_attr.retry_on_failure = false;
    Status allocation_status(context_->allocate_temp(
        DT_UINT8, TensorShape({byte_size}), &temporary_memory,
        AllocatorAttributes(), allocation_attr));
    if (!allocation_status.ok()) {
      return se::port::Status{
          se::port::error::UNAVAILABLE,
          absl::StrCat("Failed to allocate the requested memory size (",
                       byte_size, ").")};
    }
    // Hold the reference of the allocated tensors until the end of the
    // allocator.
    allocated_tensors_.push_back(temporary_memory);
    total_byte_size_ += byte_size;
    return se::port::StatusOr<se::DeviceMemory<uint8>>(
        AsDeviceMemory(temporary_memory.flat<uint8>().data(),
                       temporary_memory.flat<uint8>().size()));
  }
  int64 TotalByteSize() { return total_byte_size_; }

 private:
  int64 memory_limit_;
  int64 total_byte_size_;
  OpKernelContext* context_;
  std::vector<Tensor> allocated_tensors_;
};


typedef Eigen::GpuDevice GPUDevice;

}  // namespace tensorflow

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#endif  // TENSORFLOW_CORE_KERNELS_CONV_OPS_GPU_H_
