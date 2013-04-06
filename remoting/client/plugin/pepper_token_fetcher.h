// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_TOKEN_FETCHER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_TOKEN_FETCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/third_party_client_authenticator.h"

namespace remoting {

class ChromotingInstance;

class PepperTokenFetcher
    : public protocol::ThirdPartyClientAuthenticator::TokenFetcher {
 public:
  PepperTokenFetcher(base::WeakPtr<ChromotingInstance> plugin,
                     const std::string& host_public_key);
  virtual ~PepperTokenFetcher();

  // protocol::TokenClientAuthenticator::TokenFetcher interface.
  virtual void FetchThirdPartyToken(
      const GURL& token_url,
      const std::string& scope,
      const TokenFetchedCallback& token_fetched_callback) OVERRIDE;

  // Called by ChromotingInstance when the webapp finishes fetching the token.
  void OnTokenFetched(const std::string& token,
                      const std::string& shared_secret);

 private:
  base::WeakPtr<ChromotingInstance> plugin_;
  std::string host_public_key_;
  TokenFetchedCallback token_fetched_callback_;
  base::WeakPtrFactory<PepperTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperTokenFetcher);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_TOKEN_FETCHER_H_
