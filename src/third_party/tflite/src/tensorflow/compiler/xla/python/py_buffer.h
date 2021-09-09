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

#ifndef TENSORFLOW_COMPILER_XLA_PYTHON_PY_BUFFER_H_
#define TENSORFLOW_COMPILER_XLA_PYTHON_PY_BUFFER_H_

#include <memory>
#include <vector>

#include "tensorflow/compiler/xla/python/py_client.h"
#include "tensorflow/compiler/xla/python/traceback.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/types.h"

namespace xla {

// Python wrapper around PjRtBuffer. We use a wrapper class:
// a) to keep the PjRtClient alive via a std::shared_ptr<>
// b) to add Python-specific functionality.
class PyBuffer {
 public:
  PyBuffer(std::shared_ptr<PyClient> client, std::unique_ptr<PjRtBuffer> buffer,
           std::shared_ptr<Traceback> traceback);
  ~PyBuffer();

  std::shared_ptr<PyClient> client() const { return client_; }
  PjRtBuffer* buffer() const { return buffer_.get(); }

  ClientAndPtr<Device> device() const;
  const std::string& platform_name() const { return buffer_->platform_name(); }
  bool is_deleted() const { return buffer_->IsDeleted(); }

  StatusOr<std::unique_ptr<PyBuffer>> CopyToDevice(
      const ClientAndPtr<Device>& dst_device) const;

  void Delete() { return buffer_->Delete(); }

  Status BlockHostUntilReady();
  Status CopyToHostAsync() { return buffer_->CopyToHostAsync(); }

  const Shape& shape() { return buffer_->on_host_shape(); }

  StatusOr<std::uintptr_t> UnsafeBufferPointer() const;

  // Implementation of the CUDA array interface for sharing GPU buffers with
  // other Python libraries.
  StatusOr<pybind11::dict> CudaArrayInterface() const;

  // PEP 3118 Python buffer protocol implementation.
  static PyBufferProcs* BufferProtocol();

  Traceback* traceback() { return traceback_.get(); }

 private:
  friend class PyClient;

  std::shared_ptr<PyClient> client_;
  std::unique_ptr<PjRtBuffer> buffer_;
  std::shared_ptr<Traceback> traceback_;

  // Doubly-linked list of all buffers known to the client. Protected by the
  // GIL.
  PyBuffer* next_;
  PyBuffer* prev_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_PYTHON_PY_BUFFER_H_
