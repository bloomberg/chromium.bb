// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter.h"

#include "base/values.h"
#include "crypto/random.h"
#include "google_apis/google_api_keys.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/oauth_helper.h"

namespace {

void AppendByte(std::string& s, uint8 byte) {
  const char alphabet[] = "0123456789abcdef";
  s.push_back(alphabet[byte / 16]);
  s.push_back(alphabet[byte & 15]);
}

std::string MakeHostId() {
  static const int random_byte_num = 16;
  static const unsigned int id_len = 36;
  scoped_array<uint8> random_bytes(new uint8[random_byte_num]);
  crypto::RandBytes(&random_bytes[0], random_byte_num);
  std::string id;
  id.reserve(id_len);
  int byte_count = 0;
  for (int i = 0; i < 4; i++) {
    AppendByte(id, random_bytes[byte_count++]);
  }
  id.push_back('-');
  for (int i = 0; i < 2; i++) {
    AppendByte(id, random_bytes[byte_count++]);
  }
  id.push_back('-');
  for (int i = 0; i < 2; i++) {
    AppendByte(id, random_bytes[byte_count++]);
  }
  id.push_back('-');
  for (int i = 0; i < 2; i++) {
    AppendByte(id, random_bytes[byte_count++]);
  }
  id.push_back('-');
  for (int i = 0; i < 6; i++) {
    AppendByte(id, random_bytes[byte_count++]);
  }
  DCHECK_EQ(random_byte_num, byte_count);
  DCHECK_EQ(id_len, id.length());
  return id;
}

const int kMaxGetTokensRetries = 3;

}  // namespace

namespace remoting {

HostStarter::HostStarter(
    scoped_ptr<gaia::GaiaOAuthClient> oauth_client,
    scoped_ptr<remoting::GaiaUserEmailFetcher> user_email_fetcher,
    scoped_ptr<remoting::ServiceClient> service_client,
    scoped_ptr<remoting::DaemonController> daemon_controller)
    : oauth_client_(oauth_client.Pass()),
      user_email_fetcher_(user_email_fetcher.Pass()),
      service_client_(service_client.Pass()),
      daemon_controller_(daemon_controller.Pass()),
      in_progress_(false),
      consent_to_data_collection_(false) {
  oauth_client_info_.client_id =
    google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  oauth_client_info_.client_secret =
    google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  oauth_client_info_.redirect_uri = GetOauthRedirectUrl();
}

HostStarter::~HostStarter() {
}

scoped_ptr<HostStarter> HostStarter::Create(
    net::URLRequestContextGetter* url_request_context_getter) {
  scoped_ptr<gaia::GaiaOAuthClient> oauth_client(
      new gaia::GaiaOAuthClient(gaia::kGaiaOAuth2Url,
                                url_request_context_getter));
  scoped_ptr<remoting::GaiaUserEmailFetcher> user_email_fetcher(
      new remoting::GaiaUserEmailFetcher(url_request_context_getter));
  scoped_ptr<remoting::ServiceClient> service_client(
      new remoting::ServiceClient(url_request_context_getter));
  scoped_ptr<remoting::DaemonController> daemon_controller(
      remoting::DaemonController::Create());
  return scoped_ptr<HostStarter>(
      new HostStarter(oauth_client.Pass(), user_email_fetcher.Pass(),
                      service_client.Pass(), daemon_controller.Pass()));
}

void HostStarter::StartHost(const std::string& host_name,
                            const std::string& host_pin,
                            bool consent_to_data_collection,
                            const std::string& auth_code,
                            CompletionCallback on_done) {
  if (in_progress_) {
    on_done.Run(START_IN_PROGRESS);
    return;
  }
  in_progress_ = true;
  host_name_ = host_name;
  host_pin_ = host_pin;
  consent_to_data_collection_ = consent_to_data_collection;
  on_done_ = on_done;
  // Map the authorization code to refresh and access tokens.
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_, auth_code,
                                       kMaxGetTokensRetries, this);
}

void HostStarter::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  refresh_token_ = refresh_token;
  access_token_ = access_token;
  // Get the email corresponding to the access token.
  user_email_fetcher_->GetUserEmail(access_token_, this);
}

void HostStarter::OnGetUserEmailResponse(const std::string& user_email) {
  user_email_ = user_email;
  // Register the host.
  host_id_ = MakeHostId();
  key_pair_.Generate();
  service_client_->RegisterHost(
      host_id_, host_name_, key_pair_.GetPublicKey(), access_token_, this);
}

void HostStarter::OnHostRegistered() {
  // Start the host.
  std::string host_secret_hash = remoting::MakeHostPinHash(host_id_, host_pin_);
  scoped_ptr<base::DictionaryValue> config(new base::DictionaryValue());
  config->SetString("xmpp_login", user_email_);
  config->SetString("oauth_refresh_token", refresh_token_);
  config->SetBoolean("oauth_use_official_client_id", true);
  config->SetString("host_id", host_id_);
  config->SetString("host_name", host_name_);
  config->SetString("private_key", key_pair_.GetAsString());
  config->SetString("host_secret_hash", host_secret_hash);
  daemon_controller_->SetConfigAndStart(
      config.Pass(), consent_to_data_collection_,
      base::Bind(&HostStarter::OnHostStarted,
                 base::Unretained(this)));
}

void HostStarter::OnHostStarted(DaemonController::AsyncResult result) {
  on_done_.Run(
      (result == DaemonController::RESULT_OK) ? START_COMPLETE : START_ERROR);
  // TODO(simonmorris): Unregister the host if we didn't start it.
  in_progress_ = false;
}

void HostStarter::OnOAuthError() {
  on_done_.Run(OAUTH_ERROR);
  in_progress_ = false;
}

void HostStarter::OnNetworkError(int response_code) {
  on_done_.Run(NETWORK_ERROR);
  in_progress_ = false;
}

void HostStarter::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  NOTREACHED();
  on_done_.Run(OAUTH_ERROR);
  in_progress_ = false;
}

}  // namespace remoting
