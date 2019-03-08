// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
#define REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_client.h"

namespace remoting {

namespace test {
class TestTokenStorage;
}  // namespace test

class TestOAuthTokenGetterFactory;

class FtlSignalingPlayground {
 public:
  FtlSignalingPlayground();
  ~FtlSignalingPlayground();

  bool ShouldPrintHelp();
  void PrintHelp();
  void StartAndAuthenticate();

 private:
  void StartLoop();
  void AuthenticateAndResetClient();
  void OnAccessToken(base::OnceClosure on_done,
                     OAuthTokenGetter::Status status,
                     const std::string& user_email,
                     const std::string& access_token);
  void HandleGrpcStatusError(const grpc::Status& status);

  void GetIceServer(base::OnceClosure on_done);
  void OnGetIceServerResponse(base::OnceClosure on_done,
                              const grpc::Status& status,
                              const ftl::GetICEServerResponse& response);

  void SignInGaia(base::OnceClosure on_done);
  void OnSignInGaiaResponse(base::OnceClosure on_done,
                            const grpc::Status& status,
                            const ftl::SignInGaiaResponse& response);

  void PullMessages(base::OnceClosure on_done);
  void OnPullMessagesResponse(base::OnceClosure on_done,
                              const grpc::Status& status,
                              const ftl::PullMessagesResponse& response);
  void OnAckMessagesResponse(base::OnceClosure on_done,
                             const grpc::Status& status,
                             const ftl::AckMessagesResponse& response);

  std::unique_ptr<test::TestTokenStorage> storage_;
  std::unique_ptr<TestOAuthTokenGetterFactory> token_getter_factory_;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<FtlClient> client_;

  base::WeakPtrFactory<FtlSignalingPlayground> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlSignalingPlayground);
};

}  // namespace remoting

#endif  // REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
