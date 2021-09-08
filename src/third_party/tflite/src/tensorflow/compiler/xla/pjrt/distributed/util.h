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

#ifndef TENSORFLOW_COMPILER_XLA_PJRT_DISTRIBUTED_UTIL_H_
#define TENSORFLOW_COMPILER_XLA_PJRT_DISTRIBUTED_UTIL_H_

#include "grpcpp/support/status.h"
#include "tensorflow/compiler/xla/status.h"

namespace xla {

inline Status FromGrpcStatus(const ::grpc::Status& s) {
  if (s.ok()) {
    return Status::OK();
  } else {
    return Status(static_cast<tensorflow::error::Code>(s.error_code()),
                  s.error_message());
  }
}

inline ::grpc::Status ToGrpcStatus(const Status& s) {
  if (s.ok()) {
    return ::grpc::Status::OK;
  } else {
    return ::grpc::Status(static_cast<::grpc::StatusCode>(s.code()),
                          s.error_message());
  }
}

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_PJRT_DISTRIBUTED_UTIL_H_
