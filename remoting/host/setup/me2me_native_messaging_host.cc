// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/oauth_client.h"
#include "remoting/protocol/pairing_registry.h"

namespace {

// redirect_uri to use when authenticating service accounts (service account
// codes are obtained "out-of-band", i.e., not through an OAuth redirect).
const char* kServiceAccountRedirectUri = "oob";

// Features supported in addition to the base protocol.
const char* kSupportedFeatures[] = {
  "pairingRegistry",
  "oauthClient"
};

// Helper to extract the "config" part of a message as a DictionaryValue.
// Returns NULL on failure, and logs an error message.
scoped_ptr<base::DictionaryValue> ConfigDictionaryFromMessage(
    const base::DictionaryValue& message) {
  scoped_ptr<base::DictionaryValue> result;
  const base::DictionaryValue* config_dict;
  if (message.GetDictionary("config", &config_dict)) {
    result.reset(config_dict->DeepCopy());
  } else {
    LOG(ERROR) << "'config' dictionary not found";
  }
  return result.Pass();
}

}  // namespace

namespace remoting {

Me2MeNativeMessagingHost::Me2MeNativeMessagingHost(
    scoped_ptr<NativeMessagingChannel> channel,
    scoped_refptr<DaemonController> daemon_controller,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    scoped_ptr<OAuthClient> oauth_client)
    : channel_(channel.Pass()),
      daemon_controller_(daemon_controller),
      pairing_registry_(pairing_registry),
      oauth_client_(oauth_client.Pass()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

Me2MeNativeMessagingHost::~Me2MeNativeMessagingHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Me2MeNativeMessagingHost::Start(
      const base::Closure& quit_closure) {
  DCHECK(thread_checker_.CalledOnValidThread());

  channel_->Start(
      base::Bind(&Me2MeNativeMessagingHost::ProcessMessage, weak_ptr_),
      quit_closure);
}

void Me2MeNativeMessagingHost::ProcessMessage(
    scoped_ptr<base::DictionaryValue> message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message->Get("id", &id))
    response->Set("id", id->DeepCopy());

  std::string type;
  if (!message->GetString("type", &type)) {
    LOG(ERROR) << "'type' not found";
    channel_->SendMessage(scoped_ptr<base::DictionaryValue>());
    return;
  }

  response->SetString("type", type + "Response");

  bool success = false;
  if (type == "hello") {
    success = ProcessHello(*message, response.Pass());
  } else if (type == "clearPairedClients") {
    success = ProcessClearPairedClients(*message, response.Pass());
  } else if (type == "deletePairedClient") {
    success = ProcessDeletePairedClient(*message, response.Pass());
  } else if (type == "getHostName") {
    success = ProcessGetHostName(*message, response.Pass());
  } else if (type == "getPinHash") {
    success = ProcessGetPinHash(*message, response.Pass());
  } else if (type == "generateKeyPair") {
    success = ProcessGenerateKeyPair(*message, response.Pass());
  } else if (type == "updateDaemonConfig") {
    success = ProcessUpdateDaemonConfig(*message, response.Pass());
  } else if (type == "getDaemonConfig") {
    success = ProcessGetDaemonConfig(*message, response.Pass());
  } else if (type == "getPairedClients") {
    success = ProcessGetPairedClients(*message, response.Pass());
  } else if (type == "getUsageStatsConsent") {
    success = ProcessGetUsageStatsConsent(*message, response.Pass());
  } else if (type == "startDaemon") {
    success = ProcessStartDaemon(*message, response.Pass());
  } else if (type == "stopDaemon") {
    success = ProcessStopDaemon(*message, response.Pass());
  } else if (type == "getDaemonState") {
    success = ProcessGetDaemonState(*message, response.Pass());
  } else if (type == "getHostClientId") {
    success = ProcessGetHostClientId(*message, response.Pass());
  } else if (type == "getCredentialsFromAuthCode") {
    success = ProcessGetCredentialsFromAuthCode(*message, response.Pass());
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
  }

  if (!success)
    channel_->SendMessage(scoped_ptr<base::DictionaryValue>());
}

bool Me2MeNativeMessagingHost::ProcessHello(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("version", STRINGIZE(VERSION));
  scoped_ptr<base::ListValue> supported_features_list(new base::ListValue());
  supported_features_list->AppendStrings(std::vector<std::string>(
      kSupportedFeatures, kSupportedFeatures + arraysize(kSupportedFeatures)));
  response->Set("supportedFeatures", supported_features_list.release());
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessClearPairedClients(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pairing_registry_) {
    pairing_registry_->ClearAllPairings(
        base::Bind(&Me2MeNativeMessagingHost::SendBooleanResult, weak_ptr_,
                   base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
  return true;
}

bool Me2MeNativeMessagingHost::ProcessDeletePairedClient(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string client_id;
  if (!message.GetString(protocol::PairingRegistry::kClientIdKey, &client_id)) {
    LOG(ERROR) << "'" << protocol::PairingRegistry::kClientIdKey
               << "' string not found.";
    return false;
  }

  if (pairing_registry_) {
    pairing_registry_->DeletePairing(
        client_id, base::Bind(&Me2MeNativeMessagingHost::SendBooleanResult,
                              weak_ptr_, base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetHostName(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("hostname", net::GetHostName());
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetPinHash(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string host_id;
  if (!message.GetString("hostId", &host_id)) {
    LOG(ERROR) << "'hostId' not found: " << message;
    return false;
  }
  std::string pin;
  if (!message.GetString("pin", &pin)) {
    LOG(ERROR) << "'pin' not found: " << message;
    return false;
  }
  response->SetString("hash", MakeHostPinHash(host_id, pin));
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGenerateKeyPair(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  response->SetString("privateKey", key_pair->ToString());
  response->SetString("publicKey", key_pair->GetPublicKey());
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessUpdateDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message);
  if (!config_dict)
    return false;

  daemon_controller_->UpdateConfig(
      config_dict.Pass(),
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  daemon_controller_->GetConfig(
      base::Bind(&Me2MeNativeMessagingHost::SendConfigResponse, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetPairedClients(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pairing_registry_) {
    pairing_registry_->GetAllPairings(
        base::Bind(&Me2MeNativeMessagingHost::SendPairedClientsResponse,
                   weak_ptr_, base::Passed(&response)));
  } else {
    scoped_ptr<base::ListValue> no_paired_clients(new base::ListValue);
    SendPairedClientsResponse(response.Pass(), no_paired_clients.Pass());
  }
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetUsageStatsConsent(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&Me2MeNativeMessagingHost::SendUsageStatsConsentResponse,
                 weak_ptr_, base::Passed(&response)));
  return true;
}

bool Me2MeNativeMessagingHost::ProcessStartDaemon(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool consent;
  if (!message.GetBoolean("consent", &consent)) {
    LOG(ERROR) << "'consent' not found.";
    return false;
  }

  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message);
  if (!config_dict)
    return false;

  daemon_controller_->SetConfigAndStart(
      config_dict.Pass(), consent,
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool Me2MeNativeMessagingHost::ProcessStopDaemon(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  daemon_controller_->Stop(
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetDaemonState(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DaemonController::State state = daemon_controller_->GetState();
  switch (state) {
    case DaemonController::STATE_NOT_IMPLEMENTED:
      response->SetString("state", "NOT_IMPLEMENTED");
      break;
    case DaemonController::STATE_NOT_INSTALLED:
      response->SetString("state", "NOT_INSTALLED");
      break;
    case DaemonController::STATE_INSTALLING:
      response->SetString("state", "INSTALLING");
      break;
    case DaemonController::STATE_STOPPED:
      response->SetString("state", "STOPPED");
      break;
    case DaemonController::STATE_STARTING:
      response->SetString("state", "STARTING");
      break;
    case DaemonController::STATE_STARTED:
      response->SetString("state", "STARTED");
      break;
    case DaemonController::STATE_STOPPING:
      response->SetString("state", "STOPPING");
      break;
    case DaemonController::STATE_UNKNOWN:
      response->SetString("state", "UNKNOWN");
      break;
  }
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetHostClientId(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("clientId", google_apis::GetOAuth2ClientID(
      google_apis::CLIENT_REMOTING_HOST));
  channel_->SendMessage(response.Pass());
  return true;
}

bool Me2MeNativeMessagingHost::ProcessGetCredentialsFromAuthCode(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string auth_code;
  if (!message.GetString("authorizationCode", &auth_code)) {
    LOG(ERROR) << "'authorizationCode' string not found.";
    return false;
  }

  gaia::OAuthClientInfo oauth_client_info = {
    google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
    google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST),
    kServiceAccountRedirectUri
  };

  oauth_client_->GetCredentialsFromAuthCode(
      oauth_client_info, auth_code, base::Bind(
          &Me2MeNativeMessagingHost::SendCredentialsResponse, weak_ptr_,
          base::Passed(&response)));

  return true;
}

void Me2MeNativeMessagingHost::SendConfigResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::DictionaryValue> config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (config) {
    response->Set("config", config.release());
  } else {
    response->Set("config", Value::CreateNullValue());
  }
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendPairedClientsResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::ListValue> pairings) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->Set("pairedClients", pairings.release());
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response,
    const DaemonController::UsageStatsConsent& consent) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetBoolean("supported", consent.supported);
  response->SetBoolean("allowed", consent.allowed);
  response->SetBoolean("setByPolicy", consent.set_by_policy);
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendAsyncResult(
    scoped_ptr<base::DictionaryValue> response,
    DaemonController::AsyncResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (result) {
    case DaemonController::RESULT_OK:
      response->SetString("result", "OK");
      break;
    case DaemonController::RESULT_FAILED:
      response->SetString("result", "FAILED");
      break;
    case DaemonController::RESULT_CANCELLED:
      response->SetString("result", "CANCELLED");
      break;
    case DaemonController::RESULT_FAILED_DIRECTORY:
      response->SetString("result", "FAILED_DIRECTORY");
      break;
  }
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendBooleanResult(
    scoped_ptr<base::DictionaryValue> response, bool result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetBoolean("result", result);
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendCredentialsResponse(
    scoped_ptr<base::DictionaryValue> response,
    const std::string& user_email,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("userEmail", user_email);
  response->SetString("refreshToken", refresh_token);
  channel_->SendMessage(response.Pass());
}

}  // namespace remoting
