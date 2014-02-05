// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_
#define SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "sync/base/sync_export.h"
#include "sync/notifier/gcm_network_channel_delegate.h"
#include "sync/notifier/sync_system_resources.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace syncer {

// GCMNetworkChannel is an implementation of SyncNetworkChannel that routes
// messages through GCMProfileService.
class SYNC_EXPORT_PRIVATE GCMNetworkChannel
    : public SyncNetworkChannel,
      public net::URLFetcherDelegate,
      public base::NonThreadSafe {
 public:
  GCMNetworkChannel(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_ptr<GCMNetworkChannelDelegate> delegate);

  virtual ~GCMNetworkChannel();

  // SyncNetworkChannel implementation.
  virtual void SendEncodedMessage(const std::string& encoded_message) OVERRIDE;
  virtual void UpdateCredentials(const std::string& email,
                                 const std::string& token) OVERRIDE;

  // URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  void ResetRegisterBackoffEntryForTest(
      const net::BackoffEntry::Policy* policy);

 private:
  void Register();
  void OnRegisterComplete(const std::string& registration_id,
                          gcm::GCMClient::Result result);
  void RequestAccessToken();
  void OnGetTokenComplete(const GoogleServiceAuthError& error,
                          const std::string& token);
  GURL BuildUrl();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<GCMNetworkChannelDelegate> delegate_;

  // Message is saved until all conditions are met: there is valid
  // registration_id and access_token.
  std::string encoded_message_;

  // Access token is saved because in case of auth failure from server we need
  // to invalidate it.
  std::string access_token_;

  // GCM registration_id is requested one at startup and never refreshed until
  // next restart.
  std::string registration_id_;
  scoped_ptr<net::BackoffEntry> register_backoff_entry_;

  scoped_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<GCMNetworkChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMNetworkChannel);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_
