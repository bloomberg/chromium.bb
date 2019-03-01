// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
#define REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_client.h"

namespace remoting {

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

  void GetIceServer(base::OnceClosure on_done);
  static void OnGetIceServerResponse(base::OnceClosure on_done,
                                     grpc::Status status,
                                     const ftl::GetICEServerResponse& response);

  void SignInGaia(base::OnceClosure on_done);
  static void OnSignInGaiaResponse(base::OnceClosure on_done,
                                   grpc::Status status,
                                   const ftl::SignInGaiaResponse& response);

  std::unique_ptr<TestOAuthTokenGetterFactory> token_getter_factory_;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<FtlClient> client_;

  DISALLOW_COPY_AND_ASSIGN(FtlSignalingPlayground);
};

}  // namespace remoting

#endif  // REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
