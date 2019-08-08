// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_channel.h"

#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

#include "remoting/base/grpc_support/root_certs_prod.inc"

}  // namespace

GrpcChannelSharedPtr CreateSslChannelForEndpoint(const std::string& endpoint) {
  static const grpc::SslCredentialsOptions cred_options{certs_pem, {}, {}};
  auto channel_creds = grpc::SslCredentials(cred_options);
  return grpc::CreateChannel(endpoint, channel_creds);
}

}  // namespace remoting
