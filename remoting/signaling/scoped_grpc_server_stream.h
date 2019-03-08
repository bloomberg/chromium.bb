// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SCOPED_GRPC_SERVER_STREAM_H_
#define REMOTING_SIGNALING_SCOPED_GRPC_SERVER_STREAM_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace remoting {

namespace internal {
class GrpcAsyncServerStreamingCallDataBase;
}  // namespace internal

// A class that holds a gRPC server stream. The streaming channel will be closed
// once the holder object is deleted.
class ScopedGrpcServerStream {
 public:
  explicit ScopedGrpcServerStream(
      base::WeakPtr<internal::GrpcAsyncServerStreamingCallDataBase> call_data);
  ~ScopedGrpcServerStream();

 private:
  base::WeakPtr<internal::GrpcAsyncServerStreamingCallDataBase> call_data_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGrpcServerStream);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SCOPED_GRPC_SERVER_STREAM_H_
