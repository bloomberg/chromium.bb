// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_grpc_test_environment.h"

#include <utility>

#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/signaling/ftl_grpc_context.h"

namespace remoting {
namespace test {

FtlGrpcTestEnvironment::FtlGrpcTestEnvironment(
    std::shared_ptr<grpc::ChannelInterface> channel) {
  token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
      OAuthTokenGetter::Status::SUCCESS, "fake_user_email",
      "fake_access_token");
  context_ = std::make_unique<FtlGrpcContext>(token_getter_.get());
  context_->SetChannelForTesting(channel);
}

FtlGrpcTestEnvironment::~FtlGrpcTestEnvironment() = default;

}  // namespace test
}  // namespace remoting
