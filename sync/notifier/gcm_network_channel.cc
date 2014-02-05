// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "sync/notifier/gcm_network_channel.h"
#include "sync/notifier/gcm_network_channel_delegate.h"

namespace syncer {

namespace {

// Register backoff policy.
const net::BackoffEntry::Policy kRegisterBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000, // 2 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

GCMNetworkChannel::GCMNetworkChannel(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_ptr<GCMNetworkChannelDelegate> delegate)
    : request_context_getter_(request_context_getter),
      delegate_(delegate.Pass()),
      register_backoff_entry_(new net::BackoffEntry(&kRegisterBackoffPolicy)),
      weak_factory_(this) {
  Register();
}

GCMNetworkChannel::~GCMNetworkChannel() {
}

void GCMNetworkChannel::UpdateCredentials(
    const std::string& email,
    const std::string& token) {
  // Do nothing. We get access token by requesting it for every message.
}

void GCMNetworkChannel::ResetRegisterBackoffEntryForTest(
    const net::BackoffEntry::Policy* policy) {
  register_backoff_entry_.reset(new net::BackoffEntry(policy));
}

void GCMNetworkChannel::Register() {
  delegate_->Register(base::Bind(&GCMNetworkChannel::OnRegisterComplete,
                                 weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnRegisterComplete(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK(CalledOnValidThread());
  if (result == gcm::GCMClient::SUCCESS) {
    DCHECK(!registration_id.empty());
    DVLOG(2) << "Got registration_id";
    register_backoff_entry_->Reset();
    registration_id_ = registration_id;
    if (!encoded_message_.empty())
      RequestAccessToken();
  } else {
    DVLOG(2) << "Register failed: " << result;
    // Retry in case of transient error.
    switch (result) {
      case gcm::GCMClient::NETWORK_ERROR:
      case gcm::GCMClient::SERVER_ERROR:
      case gcm::GCMClient::TTL_EXCEEDED:
      case gcm::GCMClient::UNKNOWN_ERROR: {
        register_backoff_entry_->InformOfRequest(false);
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&GCMNetworkChannel::Register,
                       weak_factory_.GetWeakPtr()),
            register_backoff_entry_->GetTimeUntilRelease());
        break;
      }
      default:
        break;
    }
  }
}

void GCMNetworkChannel::SendEncodedMessage(const std::string& encoded_message) {
  DCHECK(CalledOnValidThread());
  DCHECK(!encoded_message.empty());
  DVLOG(2) << "SendEncodedMessage";
  encoded_message_ = encoded_message;

  if (!registration_id_.empty()) {
    RequestAccessToken();
  }
}

void GCMNetworkChannel::RequestAccessToken() {
  DCHECK(CalledOnValidThread());
  delegate_->RequestToken(base::Bind(&GCMNetworkChannel::OnGetTokenComplete,
                                     weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnGetTokenComplete(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK(CalledOnValidThread());
  if (encoded_message_.empty()) {
    // Nothing to do.
    return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Requesting access token failed. Persistent errors will be reported by
    // token service. Just drop this request, cacheinvalidations will retry
    // sending message and at that time we'll retry requesting access token.
    DVLOG(1) << "RequestAccessToken failed: " << error.ToString();
    return;
  }
  DCHECK(!token.empty());
  // Save access token in case POST fails and we need to invalidate it.
  access_token_ = token;

  DVLOG(2) << "Got access token, sending message";

  fetcher_.reset(net::URLFetcher::Create(BuildUrl(), net::URLFetcher::POST,
                                         this));
  fetcher_->SetRequestContext(request_context_getter_);
  const std::string auth_header("Authorization: Bearer " + access_token_);
  fetcher_->AddExtraRequestHeader(auth_header);
  fetcher_->SetUploadData("application/x-protobuffer", encoded_message_);
  fetcher_->Start();
  // Clear message to prevent accidentally resending it in the future.
  encoded_message_.clear();
}

void GCMNetworkChannel::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fetcher_, source);
  // Free fetcher at the end of function.
  scoped_ptr<net::URLFetcher> fetcher = fetcher_.Pass();

  net::URLRequestStatus status = fetcher->GetStatus();
  if (!status.is_success()) {
    DVLOG(1) << "URLFetcher failure";
    return;
  }

  if (fetcher->GetResponseCode() == net::HTTP_UNAUTHORIZED) {
    DVLOG(1) << "URLFetcher failure: HTTP_UNAUTHORIZED";
    delegate_->InvalidateToken(access_token_);
    return;
  }
  DVLOG(2) << "URLFetcher success";
}

GURL GCMNetworkChannel::BuildUrl() {
  DCHECK(!registration_id_.empty());
  // Prepare NetworkEndpointId using registration_id
  // Serialize NetworkEndpointId into byte array and base64 encode.
  // Format url using encoded NetworkEndpointId.
  // TODO(pavely): implement all of the above.
  return GURL("http://invalid.url.com");
}

}  // namespace syncer
