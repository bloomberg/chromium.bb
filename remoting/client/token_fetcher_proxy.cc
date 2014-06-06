// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/token_fetcher_proxy.h"

namespace remoting {

TokenFetcherProxy::TokenFetcherProxy(
    const TokenFetcherCallback& token_fetcher_impl,
    const std::string& host_public_key)
    : host_public_key_(host_public_key),
      token_fetcher_impl_(token_fetcher_impl),
      weak_factory_(this) {
}

TokenFetcherProxy::~TokenFetcherProxy() {
}

void TokenFetcherProxy::FetchThirdPartyToken(
    const GURL& token_url,
    const std::string& scope,
    const TokenFetchedCallback& token_fetched_callback) {
  token_fetched_callback_ = token_fetched_callback;
  token_fetcher_impl_.Run(
      token_url, host_public_key_, scope, weak_factory_.GetWeakPtr());
}

void TokenFetcherProxy::OnTokenFetched(
    const std::string& token, const std::string& shared_secret) {
  if (!token_fetched_callback_.is_null()) {
    token_fetched_callback_.Run(token, shared_secret);
    token_fetched_callback_.Reset();
  }
}

}  // namespace remoting
