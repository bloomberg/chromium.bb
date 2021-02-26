// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

class TrustedVaultAccessTokenFetcherFrontend;

// Must be created on the UI thread, but can be used on any sequence.
class TrustedVaultAccessTokenFetcherImpl
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit TrustedVaultAccessTokenFetcherImpl(
      base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend);
  TrustedVaultAccessTokenFetcherImpl(
      const TrustedVaultAccessTokenFetcherImpl& other) = delete;
  TrustedVaultAccessTokenFetcherImpl& operator=(
      const TrustedVaultAccessTokenFetcherImpl& other) = delete;
  ~TrustedVaultAccessTokenFetcherImpl() override;

  // TrustedVaultAccessTokenFetcher implementation.
  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override;

 private:
  base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend_;
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_
