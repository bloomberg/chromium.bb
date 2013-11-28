// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_util.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/url_request_context.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/oauth_client.h"
#include "remoting/protocol/pairing_registry.h"

namespace {

const char kParentWindowSwitchName[] = "parent-window";

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

NativeMessagingHost::NativeMessagingHost(
    scoped_refptr<DaemonController> daemon_controller,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    scoped_ptr<OAuthClient> oauth_client)
    : daemon_controller_(daemon_controller),
      pairing_registry_(pairing_registry),
      oauth_client_(oauth_client.Pass()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

NativeMessagingHost::~NativeMessagingHost() {
}

void NativeMessagingHost::SetSendMessageCallback(
    const SendMessageCallback& send_message) {
  send_message_ = send_message;
}

void NativeMessagingHost::ProcessMessage(
    scoped_ptr<base::DictionaryValue> message) {
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message->Get("id", &id))
    response->Set("id", id->DeepCopy());

  std::string type;
  if (!message->GetString("type", &type)) {
    LOG(ERROR) << "'type' not found";
    send_message_.Run(scoped_ptr<base::DictionaryValue>());
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
    send_message_.Run(scoped_ptr<base::DictionaryValue>());
}

bool NativeMessagingHost::ProcessHello(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  response->SetString("version", STRINGIZE(VERSION));
  scoped_ptr<base::ListValue> supported_features_list(new base::ListValue());
  supported_features_list->AppendStrings(std::vector<std::string>(
      kSupportedFeatures, kSupportedFeatures + arraysize(kSupportedFeatures)));
  response->Set("supportedFeatures", supported_features_list.release());
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessClearPairedClients(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  if (pairing_registry_) {
    pairing_registry_->ClearAllPairings(
        base::Bind(&NativeMessagingHost::SendBooleanResult, weak_ptr_,
                   base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
  return true;
}

bool NativeMessagingHost::ProcessDeletePairedClient(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  std::string client_id;
  if (!message.GetString(protocol::PairingRegistry::kClientIdKey, &client_id)) {
    LOG(ERROR) << "'" << protocol::PairingRegistry::kClientIdKey
               << "' string not found.";
    return false;
  }

  if (pairing_registry_) {
    pairing_registry_->DeletePairing(
        client_id, base::Bind(&NativeMessagingHost::SendBooleanResult,
                              weak_ptr_, base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
  return true;
}

bool NativeMessagingHost::ProcessGetHostName(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  response->SetString("hostname", net::GetHostName());
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessGetPinHash(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
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
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessGenerateKeyPair(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  response->SetString("privateKey", key_pair->ToString());
  response->SetString("publicKey", key_pair->GetPublicKey());
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessUpdateDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message);
  if (!config_dict)
    return false;

  daemon_controller_->UpdateConfig(
      config_dict.Pass(),
      base::Bind(&NativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->GetConfig(
      base::Bind(&NativeMessagingHost::SendConfigResponse, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetPairedClients(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  if (pairing_registry_) {
    pairing_registry_->GetAllPairings(
        base::Bind(&NativeMessagingHost::SendPairedClientsResponse, weak_ptr_,
                   base::Passed(&response)));
  } else {
    scoped_ptr<base::ListValue> no_paired_clients(new base::ListValue);
    SendPairedClientsResponse(response.Pass(), no_paired_clients.Pass());
  }
  return true;
}

bool NativeMessagingHost::ProcessGetUsageStatsConsent(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&NativeMessagingHost::SendUsageStatsConsentResponse,
                 weak_ptr_, base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessStartDaemon(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
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
      base::Bind(&NativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessStopDaemon(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->Stop(
      base::Bind(&NativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetDaemonState(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
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
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessGetHostClientId(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  response->SetString("clientId", google_apis::GetOAuth2ClientID(
      google_apis::CLIENT_REMOTING_HOST));
  send_message_.Run(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessGetCredentialsFromAuthCode(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
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
          &NativeMessagingHost::SendCredentialsResponse, weak_ptr_,
          base::Passed(&response)));

  return true;
}

void NativeMessagingHost::SendConfigResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::DictionaryValue> config) {
  if (config) {
    response->Set("config", config.release());
  } else {
    response->Set("config", Value::CreateNullValue());
  }
  send_message_.Run(response.Pass());
}

void NativeMessagingHost::SendPairedClientsResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::ListValue> pairings) {
  response->Set("pairedClients", pairings.release());
  send_message_.Run(response.Pass());
}

void NativeMessagingHost::SendUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response,
    const DaemonController::UsageStatsConsent& consent) {
  response->SetBoolean("supported", consent.supported);
  response->SetBoolean("allowed", consent.allowed);
  response->SetBoolean("setByPolicy", consent.set_by_policy);
  send_message_.Run(response.Pass());
}

void NativeMessagingHost::SendAsyncResult(
    scoped_ptr<base::DictionaryValue> response,
    DaemonController::AsyncResult result) {
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
  send_message_.Run(response.Pass());
}

void NativeMessagingHost::SendBooleanResult(
    scoped_ptr<base::DictionaryValue> response, bool result) {
  response->SetBoolean("result", result);
  send_message_.Run(response.Pass());
}

void NativeMessagingHost::SendCredentialsResponse(
    scoped_ptr<base::DictionaryValue> response,
    const std::string& user_email,
    const std::string& refresh_token) {
  response->SetString("userEmail", user_email);
  response->SetString("refreshToken", refresh_token);
  send_message_.Run(response.Pass());
}

int NativeMessagingHostMain() {
#if defined(OS_WIN)
  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the returned
  // handles are invalid in that case unless standard input and output are
  // redirected to a pipe or file.
  base::PlatformFile read_file = GetStdHandle(STD_INPUT_HANDLE);
  base::PlatformFile write_file = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(OS_POSIX)
  base::PlatformFile read_file = STDIN_FILENO;
  base::PlatformFile write_file = STDOUT_FILENO;
#else
#error Not implemented.
#endif

  // Mac OS X requires that the main thread be a UI message loop in order to
  // receive distributed notifications from the System Preferences pane. An
  // IO thread is needed for the pairing registry and URL context getter.
  base::Thread io_thread("io_thread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  base::MessageLoopForUI message_loop;
  base::RunLoop run_loop;

  scoped_refptr<DaemonController> daemon_controller =
      DaemonController::Create();

  // Pass handle of the native view to the controller so that the UAC prompts
  // are focused properly.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kParentWindowSwitchName)) {
    std::string native_view =
        command_line->GetSwitchValueASCII(kParentWindowSwitchName);
    int64 native_view_handle = 0;
    if (base::StringToInt64(native_view, &native_view_handle)) {
      daemon_controller->SetWindow(reinterpret_cast<void*>(native_view_handle));
    } else {
      LOG(WARNING) << "Invalid parameter value --" << kParentWindowSwitchName
                   << "=" << native_view;
    }
  }

  // OAuth client (for credential requests).
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new URLRequestContextGetter(io_thread.message_loop_proxy()));
  scoped_ptr<OAuthClient> oauth_client(
      new OAuthClient(url_request_context_getter));

  net::URLFetcher::SetIgnoreCertificateRequests(true);

  // Create the pairing registry and native messaging host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry =
      CreatePairingRegistry(io_thread.message_loop_proxy());
  scoped_ptr<NativeMessagingChannel::Delegate> host(
      new NativeMessagingHost(daemon_controller,
                              pairing_registry,
                              oauth_client.Pass()));

  // Set up the native messaging channel.
  scoped_ptr<NativeMessagingChannel> channel(
      new NativeMessagingChannel(host.Pass(), read_file, write_file));
  channel->Start(run_loop.QuitClosure());

  // Run the loop until channel is alive.
  run_loop.Run();
  return kSuccessExitCode;
}

}  // namespace remoting
