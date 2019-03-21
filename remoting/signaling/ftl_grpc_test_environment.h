// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_GRPC_TEST_ENVIRONMENT_H_
#define REMOTING_SIGNALING_FTL_GRPC_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"

namespace grpc {
class ChannelInterface;
}  // namespace grpc

namespace remoting {

class FakeOAuthTokenGetter;
class FtlGrpcContext;

namespace test {

// This class creates FtlGrpcContext with fake OAuthTokenGetter and connects it
// with the test channel.
class FtlGrpcTestEnvironment final {
 public:
  // TODO(yuweih): Typedef std::shared_ptr since it's not used in Chromium.
  explicit FtlGrpcTestEnvironment(
      std::shared_ptr<grpc::ChannelInterface> channel);
  ~FtlGrpcTestEnvironment();

  FtlGrpcContext* context() { return context_.get(); }

 private:
  std::unique_ptr<FakeOAuthTokenGetter> token_getter_;
  std::unique_ptr<FtlGrpcContext> context_;

  DISALLOW_COPY_AND_ASSIGN(FtlGrpcTestEnvironment);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_GRPC_TEST_ENVIRONMENT_H_
