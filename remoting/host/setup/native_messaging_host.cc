// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/native_messaging_host.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/strings/stringize_macros.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/pin_hash.h"

namespace {

// Helper to extract the "config" part of a message as a DictionaryValue.
// Returns NULL on failure, and logs an error message.
scoped_ptr<base::DictionaryValue> ConfigDictionaryFromMessage(
    const base::DictionaryValue& message) {
  scoped_ptr<base::DictionaryValue> config_dict;
  std::string config_str;
  if (!message.GetString("config", &config_str)) {
    LOG(ERROR) << "'config' not found.";
    return config_dict.Pass();
  }

  // TODO(lambroslambrou): Fix the webapp to embed the config dictionary
  // directly into the request, rather than as a serialized JSON string.
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(config_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!config || !config->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Bad config parameter.";
    return config_dict.Pass();
  }
  config_dict.reset(reinterpret_cast<base::DictionaryValue*>(config.release()));
  return config_dict.Pass();
}

}  // namespace

namespace remoting {

NativeMessagingHost::NativeMessagingHost(
    base::PlatformFile input,
    base::PlatformFile output,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    const base::Closure& quit_closure)
    : caller_task_runner_(caller_task_runner),
      quit_closure_(quit_closure),
      native_messaging_reader_(input),
      native_messaging_writer_(output),
      daemon_controller_(DaemonController::Create()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

NativeMessagingHost::~NativeMessagingHost() {
}

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
  const base::DictionaryValue* message_dict;
  if (!message->GetAsDictionary(&message_dict)) {
    LOG(ERROR) << "Expected DictionaryValue";
    Shutdown();
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
  SendResponse(response.Pass());
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
  response->SetString("private_key", key_pair->ToString());
  response->SetString("public_key", key_pair->GetPublicKey());
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
  daemon_controller_->GetConfig(base::Bind(
      &NativeMessagingHost::SendConfigResponse,
      base::Unretained(this), base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetUsageStatsConsent(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  daemon_controller_->GetUsageStatsConsent(base::Bind(
      &NativeMessagingHost::SendUsageStatsConsentResponse,
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
  daemon_controller_->Stop(base::Bind(
      &NativeMessagingHost::SendAsyncResult, base::Unretained(this),
      base::Passed(&response)));
  return true;
}

bool NativeMessagingHost::ProcessGetDaemonState(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  // TODO(lambroslambrou): Send the state as a string instead of an integer,
  // and update the web-app accordingly.
  DaemonController::State state = daemon_controller_->GetState();
  response->SetInteger("state", state);
  SendResponse(response.Pass());
  return true;
}

void NativeMessagingHost::SendResponse(
    scoped_ptr<base::DictionaryValue> response) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(FROM_HERE, base::Bind(
        &NativeMessagingHost::SendResponse, weak_ptr_,
        base::Passed(&response)));
    return;
  }

  if (!native_messaging_writer_.WriteMessage(*response))
    Shutdown();
}

void NativeMessagingHost::SendConfigResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::DictionaryValue> config) {
  // TODO(lambroslambrou): Fix the web-app to accept the config dictionary
  // directly embedded in the response, rather than as serialized JSON. See
  // http://crbug.com/232135.
  std::string config_json;
  base::JSONWriter::Write(config.get(), &config_json);
  response->SetString("config", config_json);
  SendResponse(response.Pass());
}

void NativeMessagingHost::SendUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response,
    bool supported,
    bool allowed,
    bool set_by_policy) {
  response->SetBoolean("supported", supported);
  response->SetBoolean("allowed", allowed);
  response->SetBoolean("set_by_policy", set_by_policy);
  SendResponse(response.Pass());
}

void NativeMessagingHost::SendAsyncResult(
    scoped_ptr<base::DictionaryValue> response,
    DaemonController::AsyncResult result) {
  // TODO(lambroslambrou): Send the result as a string instead of an integer,
  // and update the web-app accordingly. See http://crbug.com/232135.
  response->SetInteger("result", result);
  SendResponse(response.Pass());
}

}  // namespace remoting
