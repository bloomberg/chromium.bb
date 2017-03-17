// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_pairing_secret_fetcher.h"
#include "base/bind.h"
#include "base/location.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/jni/jni_client.h"

namespace remoting {

JniPairingSecretFetcher::JniPairingSecretFetcher(
    base::WeakPtr<JniClient> client,
    const std::string& host_id)
    : jni_client_(client), host_id_(host_id), weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

JniPairingSecretFetcher::~JniPairingSecretFetcher() {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());
}

void JniPairingSecretFetcher::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  callback_ = callback;
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniPairingSecretFetcher::FetchSecretOnUiThread,
                            jni_client_, host_id_, pairable));
}

void JniPairingSecretFetcher::ProvideSecret(const std::string& pin) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!callback_.is_null());

  callback_.Run(pin);
}

base::WeakPtr<JniPairingSecretFetcher> JniPairingSecretFetcher::GetWeakPtr() {
  return weak_ptr_;
}

// static
void JniPairingSecretFetcher::FetchSecretOnUiThread(
    base::WeakPtr<JniClient> client,
    const std::string& host_id,
    bool pairable) {
  if (!client) {
    return;
  }

  // Delete pairing credentials if they exist.
  client->CommitPairingCredentials(host_id, "", "");

  client->DisplayAuthenticationPrompt(pairable);
}

}  // namespace remoting
