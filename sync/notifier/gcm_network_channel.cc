// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/strings/string_util.h"
#if !defined(ANDROID)
// channel_common.proto defines ANDROID constant that conflicts with Android
// build. At the same time TiclInvalidationService is not used on Android so it
// is safe to exclude these protos from Android build.
#include "google/cacheinvalidation/android_channel.pb.h"
#include "google/cacheinvalidation/channel_common.pb.h"
#endif
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "sync/notifier/gcm_network_channel.h"
#include "sync/notifier/gcm_network_channel_delegate.h"

namespace syncer {

namespace {

const char kCacheInvalidationEndpointUrl[] =
    "https://clients4.google.com/invalidation/android/request/";
const char kCacheInvalidationPackageName[] = "com.google.chrome.invalidations";

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
  delegate_->Initialize();
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
    if (!cached_message_.empty())
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

void GCMNetworkChannel::SendMessage(const std::string& message) {
  DCHECK(CalledOnValidThread());
  DCHECK(!message.empty());
  DVLOG(2) << "SendMessage";
  cached_message_ = message;

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
  if (cached_message_.empty()) {
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
  fetcher_.reset(net::URLFetcher::Create(
      BuildUrl(registration_id_), net::URLFetcher::POST, this));
  fetcher_->SetRequestContext(request_context_getter_);
  const std::string auth_header("Authorization: Bearer " + access_token_);
  fetcher_->AddExtraRequestHeader(auth_header);
  fetcher_->SetUploadData("application/x-protobuffer", cached_message_);
  fetcher_->Start();
  // Clear message to prevent accidentally resending it in the future.
  cached_message_.clear();
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

GURL GCMNetworkChannel::BuildUrl(const std::string& registration_id) {
  DCHECK(!registration_id.empty());

#if !defined(ANDROID)
  ipc::invalidation::EndpointId endpoint_id;
  endpoint_id.set_c2dm_registration_id(registration_id);
  endpoint_id.set_client_key(std::string());
  endpoint_id.set_package_name(kCacheInvalidationPackageName);
  endpoint_id.mutable_channel_version()->set_major_version(
      ipc::invalidation::INITIAL);
  std::string endpoint_id_buffer;
  endpoint_id.SerializeToString(&endpoint_id_buffer);

  ipc::invalidation::NetworkEndpointId network_endpoint_id;
  network_endpoint_id.set_network_address(
      ipc::invalidation::NetworkEndpointId_NetworkAddress_ANDROID);
  network_endpoint_id.set_client_address(endpoint_id_buffer);
  std::string network_endpoint_id_buffer;
  network_endpoint_id.SerializeToString(&network_endpoint_id_buffer);

  std::string base64URLPiece;
  Base64EncodeURLSafe(network_endpoint_id_buffer, &base64URLPiece);

  std::string url(kCacheInvalidationEndpointUrl);
  url += base64URLPiece;
  return GURL(url);
#else
  // This code shouldn't be invoked on Android.
  NOTREACHED();
  return GURL();
#endif
}

void GCMNetworkChannel::Base64EncodeURLSafe(const std::string& input,
                                            std::string* output) {
  base::Base64Encode(input, output);
  // Covert to url safe alphabet.
  base::ReplaceChars(*output, "+", "-", output);
  base::ReplaceChars(*output, "/", "_", output);
  // Trim padding.
  size_t padding_size = 0;
  for (size_t i = output->size(); i > 0 && (*output)[i - 1] == '='; --i)
    ++padding_size;
  output->resize(output->size() - padding_size);
}

bool GCMNetworkChannel::Base64DecodeURLSafe(const std::string& input,
                                            std::string* output) {
  // Add padding.
  size_t padded_size = (input.size() + 3) - (input.size() + 3) % 4;
  std::string padded_input(input);
  padded_input.resize(padded_size, '=');
  // Convert to standard base64 alphabet.
  base::ReplaceChars(padded_input, "-", "+", &padded_input);
  base::ReplaceChars(padded_input, "_", "/", &padded_input);
  return base::Base64Decode(padded_input, output);
}

}  // namespace syncer
