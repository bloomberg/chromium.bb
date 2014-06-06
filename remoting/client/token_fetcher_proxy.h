// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_TOKEN_FETCHER_PROXY_H_
#define REMOTING_CLIENT_TOKEN_FETCHER_PROXY_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/third_party_client_authenticator.h"

namespace remoting {

class TokenFetcherProxy
    : public protocol::ThirdPartyClientAuthenticator::TokenFetcher {
 public:
  typedef base::Callback<void(
      const GURL& token_url,
      const std::string& host_public_key,
      const std::string& scope,
      base::WeakPtr<TokenFetcherProxy>)> TokenFetcherCallback;

  TokenFetcherProxy(const TokenFetcherCallback& token_fetcher_impl,
                    const std::string& host_public_key);
  virtual ~TokenFetcherProxy();

  // protocol::TokenClientAuthenticator::TokenFetcher interface.
  virtual void FetchThirdPartyToken(
      const GURL& token_url,
      const std::string& scope,
      const TokenFetchedCallback& token_fetched_callback) OVERRIDE;

  // Called by the token fetching implementation when the token is fetched.
  void OnTokenFetched(const std::string& token,
                      const std::string& shared_secret);

 private:
  std::string host_public_key_;
  TokenFetcherCallback token_fetcher_impl_;
  TokenFetchedCallback token_fetched_callback_;
  base::WeakPtrFactory<TokenFetcherProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TokenFetcherProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_TOKEN_FETCHER_PROXY_H_
