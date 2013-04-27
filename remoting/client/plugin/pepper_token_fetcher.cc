// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_token_fetcher.h"

#include "remoting/client/plugin/chromoting_instance.h"

namespace remoting {

PepperTokenFetcher::PepperTokenFetcher(base::WeakPtr<ChromotingInstance> plugin,
                                       const std::string& host_public_key)
    : plugin_(plugin),
      host_public_key_(host_public_key),
      weak_factory_(this) {
}

PepperTokenFetcher::~PepperTokenFetcher() {
}

void PepperTokenFetcher::FetchThirdPartyToken(
    const GURL& token_url,
    const std::string& scope,
    const TokenFetchedCallback& token_fetched_callback) {
  if (plugin_) {
    token_fetched_callback_ = token_fetched_callback;
    plugin_->FetchThirdPartyToken(token_url, host_public_key_, scope,
                                  weak_factory_.GetWeakPtr());
  }
}

void PepperTokenFetcher::OnTokenFetched(
    const std::string& token, const std::string& shared_secret) {
  if (!token_fetched_callback_.is_null()) {
    token_fetched_callback_.Run(token, shared_secret);
    token_fetched_callback_.Reset();
  }
}

}  // namespace remoting
