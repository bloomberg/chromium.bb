// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_PAIRING_SECRET_FETCHER_H_
#define REMOTING_CLIENT_JNI_JNI_PAIRING_SECRET_FETCHER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/client_authentication_config.h"

namespace remoting {

class ChromotingJniRuntime;
class JniClient;

// This class fetches the pairing secret on the UI thread. This should be
// created on the UI thread but there after used and deleted on the network
// thread.
class JniPairingSecretFetcher {
 public:
  JniPairingSecretFetcher(ChromotingJniRuntime* runtime,
                   base::WeakPtr<JniClient> client,
                   const std::string& host_id);
  virtual ~JniPairingSecretFetcher();

  // Notifies the user interface that the user needs to enter a PIN. The current
  // authentication attempt is put on hold until |callback| is invoked.
  void FetchSecret(bool pairable,
                   const protocol::SecretFetchedCallback& callback);

  // Provides the user's PIN and resumes the host authentication attempt. Call
  // once the user has finished entering this PIN into the UI, but only after
  // the UI has been asked to provide a PIN (via FetchSecret()).
  void ProvideSecret(const std::string& pin);

  // Get weak pointer to be used on the network thread.
  base::WeakPtr<JniPairingSecretFetcher> GetWeakPtr();

 private:
  static void FetchSecretOnUiThread(base::WeakPtr<JniClient> client,
                                    const std::string& host_id,
                                    bool pairable);

  ChromotingJniRuntime* jni_runtime_;
  base::WeakPtr<JniClient> jni_client_;

  std::string host_id_;

  // Pass this the user's PIN once we have it. To be assigned and accessed on
  // the UI thread, but must be posted to the network thread to call it.
  protocol::SecretFetchedCallback callback_;

  base::WeakPtr<JniPairingSecretFetcher> weak_ptr_;
  base::WeakPtrFactory<JniPairingSecretFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniPairingSecretFetcher);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_PAIRING_SECRET_FETCHER_H_
