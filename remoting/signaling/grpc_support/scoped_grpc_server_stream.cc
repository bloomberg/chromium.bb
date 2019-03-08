// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"

#include "remoting/signaling/grpc_support/grpc_async_server_streaming_call_data.h"

namespace remoting {

ScopedGrpcServerStream::ScopedGrpcServerStream(
    base::WeakPtr<internal::GrpcAsyncServerStreamingCallDataBase> call_data)
    : call_data_(call_data) {}

ScopedGrpcServerStream::~ScopedGrpcServerStream() {
  if (call_data_) {
    call_data_->CancelRequest();
  }
}

}  // namespace remoting
