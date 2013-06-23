// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "crypto/random.h"

namespace {
const char kCreatedTimeKey[] = "created-time";
const char kClientIdKey[] = "client-id";
const char kClientNameKey[] = "client-name";
const char kSharedSecretKey[] = "shared-secret";
}  // namespace

namespace remoting {
namespace protocol {

// How many bytes of random data to use for the shared secret.
const int kKeySize = 16;

PairingRegistry::Pairing::Pairing() {
}

PairingRegistry::Pairing::Pairing(const base::Time& created_time,
                                  const std::string& client_name,
                                  const std::string& client_id,
                                  const std::string& shared_secret)
    : created_time_(created_time),
      client_name_(client_name),
      client_id_(client_id),
      shared_secret_(shared_secret) {
}

PairingRegistry::Pairing PairingRegistry::Pairing::Create(
    const std::string& client_name) {
  base::Time created_time = base::Time::Now();
  std::string client_id = base::GenerateGUID();
  std::string shared_secret;
  char buffer[kKeySize];
  crypto::RandBytes(buffer, arraysize(buffer));
  if (!base::Base64Encode(base::StringPiece(buffer, arraysize(buffer)),
                          &shared_secret)) {
    LOG(FATAL) << "Base64Encode failed.";
  }
  return Pairing(created_time, client_name, client_id, shared_secret);
}

PairingRegistry::Pairing::~Pairing() {
}

bool PairingRegistry::Pairing::operator==(const Pairing& other) const {
  return created_time_ == other.created_time_ &&
         client_id_ == other.client_id_ &&
         client_name_ == other.client_name_ &&
         shared_secret_ == other.shared_secret_;
}

bool PairingRegistry::Pairing::is_valid() const {
  return !client_id_.empty() && !shared_secret_.empty();
}

PairingRegistry::PairingRegistry(scoped_ptr<Delegate> delegate)
    : delegate_(delegate.Pass()) {
  DCHECK(delegate_);
}

PairingRegistry::~PairingRegistry() {
}

PairingRegistry::Pairing PairingRegistry::CreatePairing(
    const std::string& client_name) {
  DCHECK(CalledOnValidThread());
  Pairing result = Pairing::Create(client_name);
  AddPairing(result);
  return result;
}

void PairingRegistry::GetPairing(const std::string& client_id,
                                 const GetPairingCallback& callback) {
  DCHECK(CalledOnValidThread());
  delegate_->Load(
      base::Bind(&PairingRegistry::DoGetPairing, this, client_id, callback));
}

void PairingRegistry::AddPairing(const Pairing& pairing) {
  delegate_->Load(
      base::Bind(&PairingRegistry::MergePairingAndSave, this, pairing));
}

void PairingRegistry::MergePairingAndSave(const Pairing& pairing,
                                          const std::string& pairings_json) {
  DCHECK(CalledOnValidThread());
  PairedClients clients = DecodeJson(pairings_json);
  clients[pairing.client_id()] = pairing;
  std::string new_pairings_json = EncodeJson(clients);
  delegate_->Save(new_pairings_json, SaveCallback());
}

void PairingRegistry::DoGetPairing(const std::string& client_id,
                                   const GetPairingCallback& callback,
                                   const std::string& pairings_json) {
  PairedClients clients = DecodeJson(pairings_json);
  Pairing result = clients[client_id];
  callback.Run(result);
}

PairingRegistry::PairedClients PairingRegistry::DecodeJson(
    const std::string& pairings_json) {
  PairedClients result;

  if (pairings_json.empty()) {
    return result;
  }

  JSONStringValueSerializer registry(pairings_json);
  int error_code;
  std::string error_message;
  scoped_ptr<base::Value> root(
      registry.Deserialize(&error_code, &error_message));
  if (!root) {
    LOG(ERROR) << "Failed to load paired clients: " << error_message
               << " (" << error_code << ").";
    return result;
  }

  base::ListValue* root_list = NULL;
  if (!root->GetAsList(&root_list)) {
    LOG(ERROR) << "Failed to load paired clients: root node is not a list.";
    return result;
  }

  for (size_t i = 0; i < root_list->GetSize(); ++i) {
    base::DictionaryValue* pairing = NULL;
    std::string client_name, client_id, shared_secret;
    double created_time_value;
    if (root_list->GetDictionary(i, &pairing) &&
        pairing->GetDouble(kCreatedTimeKey, &created_time_value) &&
        pairing->GetString(kClientNameKey, &client_name) &&
        pairing->GetString(kClientIdKey, &client_id) &&
        pairing->GetString(kSharedSecretKey, &shared_secret)) {
      base::Time created_time = base::Time::FromJsTime(created_time_value);
      result[client_id] = Pairing(
          created_time, client_name, client_id, shared_secret);
    } else {
      LOG(ERROR) << "Paired client " << i << " has unexpected format.";
    }
  }

  return result;
}

std::string PairingRegistry::EncodeJson(const PairedClients& clients) {
  base::ListValue root;
  for (PairedClients::const_iterator i = clients.begin();
       i != clients.end(); ++i) {
    base::DictionaryValue* pairing = new base::DictionaryValue();
    pairing->SetDouble(kCreatedTimeKey, i->second.created_time().ToJsTime());
    pairing->SetString(kClientNameKey, i->second.client_name());
    pairing->SetString(kClientIdKey, i->second.client_id());
    pairing->SetString(kSharedSecretKey, i->second.shared_secret());
    root.Append(pairing);
  }

  std::string result;
  JSONStringValueSerializer serializer(&result);
  serializer.Serialize(root);

  return result;
}

}  // namespace protocol
}  // namespace remoting
