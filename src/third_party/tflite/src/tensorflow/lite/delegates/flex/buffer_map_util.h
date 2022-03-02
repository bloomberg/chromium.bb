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
#ifndef TENSORFLOW_LITE_DELEGATES_FLEX_BUFFER_MAP_UTIL_H_
#define TENSORFLOW_LITE_DELEGATES_FLEX_BUFFER_MAP_UTIL_H_

#include "tensorflow/core/framework/allocation_description.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/lite/c/common.h"

namespace tflite {
namespace flex {

// A tensor buffer that is allocated, deallocated and populated by TF Lite.
class BaseTfLiteTensorBuffer : public tensorflow::TensorBuffer {
  using tensorflow::TensorBuffer::TensorBuffer;

  inline TensorBuffer* root_buffer() override { return this; }

  void FillAllocationDescription(
      tensorflow::AllocationDescription* proto) const override;

  // Prevents input forwarding from mutating this buffer.
  inline bool OwnsMemory() const override { return false; }

 protected:
  void LogAllocation();
  void LogDeallocation();
};

// A tensor buffer for most data types. Numeric types have exactly the same
// representation in TFLITE and TF, so we just need use memcpy().
class TfLiteTensorBuffer : public BaseTfLiteTensorBuffer {
 public:
  explicit TfLiteTensorBuffer(const TfLiteTensor* tensor);

  ~TfLiteTensorBuffer() override;

  inline size_t size() const override { return len_; }

 private:
  size_t len_;
};

// A string buffer. TFLITE string tensor format is different than
// TF's so we need perform the conversion here.
class StringTfLiteTensorBuffer : public BaseTfLiteTensorBuffer {
 public:
  explicit StringTfLiteTensorBuffer(const TfLiteTensor* tensor);

  ~StringTfLiteTensorBuffer() override;

  inline size_t size() const override {
    return num_strings_ * sizeof(tensorflow::tstring);
  }

 private:
  StringTfLiteTensorBuffer(const TfLiteTensor* tensor, int num_strings);

  int num_strings_;
};

// Sets the `tensorflow::Tensor` content from `TfLiteTensor` object.
tensorflow::Status SetTfTensorFromTfLite(const TfLiteTensor* tensor,
                                         tensorflow::Tensor* tf_tensor);

}  // namespace flex
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_FLEX_BUFFER_MAP_UTIL_H_
