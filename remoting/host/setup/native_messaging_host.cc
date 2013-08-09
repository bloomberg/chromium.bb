// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/native_messaging_host.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringize_macros.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pin_hash.h"
#include "remoting/protocol/pairing_registry.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace {

// Features supported in addition to the base protocol.
const char* kSupportedFeatures[] = {
  "pairingRegistry",
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
    scoped_ptr<DaemonController> daemon_controller,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    base::PlatformFile input,
    base::PlatformFile output,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    const base::Closure& quit_closure)
    : caller_task_runner_(caller_task_runner),
      quit_closure_(quit_closure),
      native_messaging_reader_(input),
      native_messaging_writer_(output),
      daemon_controller_(daemon_controller.Pass()),
      pairing_registry_(pairing_registry),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

NativeMessagingHost::~NativeMessagingHost() {}

void NativeMessagingHost::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  native_messaging_reader_.Start(
      base::Bind(&NativeMessagingHost::ProcessMessage, weak_ptr_),
      base::Bind(&NativeMessagingHost::Shutdown, weak_ptr_));
}

void NativeMessagingHost::Shutdown() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (!quit_closure_.is_null()) {
    caller_task_runner_->PostTask(FROM_HERE, quit_closure_);
    quit_closure_.Reset();
  }
}

void NativeMessagingHost::ProcessMessage(scoped_ptr<base::Value> message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Don't process any more messages if Shutdown() has been called.
  if (quit_closure_.is_null())
    return;

  const base::DictionaryValue* message_dict;
  if (!message->GetAsDictionary(&message_dict)) {
    LOG(ERROR) << "Expected DictionaryValue";
    Shutdown();
    return;
  }

  scoped_ptr<base::DictionaryValue> response_dict(new base::DictionaryValue());

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message_dict->Get("id", &id))
    response_dict->Set("id", id->DeepCopy());

  std::string type;
  if (!message_dict->GetString("type", &type)) {
    LOG(ERROR) << "'type' not found";
    Shutdown();
    return;
  }

  response_dict->SetString("type", type + "Response");

  bool success = false;
  if (type == "hello") {
    success = ProcessHello(*message_dict, response_dict.Pass());
  } else if (type == "clearPairedClients") {
    success = ProcessClearPairedClients(*message_dict, response_dict.Pass());
  } else if (type == "deletePairedClient") {
    success = ProcessDeletePairedClient(*message_dict, response_dict.Pass());
  } else if (type == "getHostName") {
    success = ProcessGetHostName(*message_dict, response_dict.Pass());
  } else if (type == "getPinHash") {
    success = ProcessGetPinHash(*message_dict, response_dict.Pass());
  } else if (type == "generateKeyPair") {
    success = ProcessGenerateKeyPair(*message_dict, response_dict.Pass());
  } else if (type == "updateDaemonConfig") {
    success = ProcessUpdateDaemonConfig(*message_dict, response_dict.Pass());
  } else if (type == "getDaemonConfig") {
    success = ProcessGetDaemonConfig(*message_dict, response_dict.Pass());
  } else if (type == "getPairedClients") {
    success = ProcessGetPairedClients(*message_dict, response_dict.Pass());
  } else if (type == "getUsageStatsConsent") {
    success = ProcessGetUsageStatsConsent(*message_dict, response_dict.Pass());
  } else if (type == "startDaemon") {
    success = ProcessStartDaemon(*message_dict, response_dict.Pass());
  } else if (type == "stopDaemon") {
    success = ProcessStopDaemon(*message_dict, response_dict.Pass());
  } else if (type == "getDaemonState") {
    success = ProcessGetDaemonState(*message_dict, response_dict.Pass());
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
  }

  if (!success)
    Shutdown();
}

bool NativeMessagingHost::ProcessHello(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  response->SetString("version", STRINGIZE(VERSION));
  scoped_ptr<base::ListValue> supported_features_list(new base::ListValue());
  supported_features_list->AppendStrings(std::vector<std::string>(
      kSupportedFeatures, kSupportedFeatures + arraysize(kSupportedFeatures)));
  response->Set("supportedFeatures", supported_features_list.release());
  SendResponse(response.Pass());
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
  SendResponse(response.Pass());
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
  response->SetString("hash", remoting::MakeHostPinHash(host_id, pin));
  SendResponse(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessGenerateKeyPair(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  response->SetString("privateKey", key_pair->ToString());
  response->SetString("publicKey", key_pair->GetPublicKey());
  SendResponse(response.Pass());
  return true;
}

bool NativeMessagingHost::ProcessUpdateDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message);
  if (!config_dict)
    return false;

  // base::Unretained() is safe because this object owns |daemon_controller_|
  // which owns the thread that will run the callback.
  daemon_controller_->UpdateConfig(
      config_dict.Pass(),
      base::Bind(&NativeMessagingHost::SendAsyncResult, base::Unretained(this),
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetDaemonConfig(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->GetConfig(
      base::Bind(&NativeMessagingHost::SendConfigResponse,
                 base::Unretained(this), base::Passed(&response)));
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
                 base::Unretained(this), base::Passed(&response)));
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
      base::Bind(&NativeMessagingHost::SendAsyncResult, base::Unretained(this),
                 base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessStopDaemon(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->Stop(
      base::Bind(&NativeMessagingHost::SendAsyncResult, base::Unretained(this),
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
  SendResponse(response.Pass());
  return true;
}

void NativeMessagingHost::SendResponse(
    scoped_ptr<base::DictionaryValue> response) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&NativeMessagingHost::SendResponse, weak_ptr_,
                              base::Passed(&response)));
    return;
  }

  if (!native_messaging_writer_.WriteMessage(*response))
    Shutdown();
}

void NativeMessagingHost::SendConfigResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::DictionaryValue> config) {
  if (config) {
    response->Set("config", config.release());
  } else {
    response->Set("config", Value::CreateNullValue());
  }
  SendResponse(response.Pass());
}

void NativeMessagingHost::SendPairedClientsResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::ListValue> pairings) {
  response->Set("pairedClients", pairings.release());
  SendResponse(response.Pass());
}

void NativeMessagingHost::SendUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response,
    bool supported,
    bool allowed,
    bool set_by_policy) {
  response->SetBoolean("supported", supported);
  response->SetBoolean("allowed", allowed);
  response->SetBoolean("setByPolicy", set_by_policy);
  SendResponse(response.Pass());
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
  SendResponse(response.Pass());
}

void NativeMessagingHost::SendBooleanResult(
    scoped_ptr<base::DictionaryValue> response, bool result) {
  response->SetBoolean("result", result);
  SendResponse(response.Pass());
}

int NativeMessagingHostMain() {
#if defined(OS_WIN)
  base::PlatformFile read_file = GetStdHandle(STD_INPUT_HANDLE);
  base::PlatformFile write_file = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(OS_POSIX)
  base::PlatformFile read_file = STDIN_FILENO;
  base::PlatformFile write_file = STDOUT_FILENO;
#else
#error Not implemented.
#endif

  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
  base::RunLoop run_loop;
  scoped_refptr<protocol::PairingRegistry> pairing_registry =
      CreatePairingRegistry(message_loop.message_loop_proxy());
  remoting::NativeMessagingHost host(remoting::DaemonController::Create(),
                                     pairing_registry,
                                     read_file, write_file,
                                     message_loop.message_loop_proxy(),
                                     run_loop.QuitClosure());
  host.Start();
  run_loop.Run();
  return kSuccessExitCode;
}

}  // namespace remoting
