// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include <stddef.h>

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "crypto/random.h"

namespace remoting {
namespace protocol {

// How many bytes of random data to use for the shared secret.
const int kKeySize = 16;

const char PairingRegistry::kCreatedTimeKey[] = "createdTime";
const char PairingRegistry::kClientIdKey[] = "clientId";
const char PairingRegistry::kClientNameKey[] = "clientName";
const char PairingRegistry::kSharedSecretKey[] = "sharedSecret";

PairingRegistry::Pairing::Pairing() = default;

PairingRegistry::Pairing::Pairing(const base::Time& created_time,
                                  const std::string& client_name,
                                  const std::string& client_id,
                                  const std::string& shared_secret)
    : created_time_(created_time),
      client_name_(client_name),
      client_id_(client_id),
      shared_secret_(shared_secret) {
}

PairingRegistry::Pairing::Pairing(const Pairing& other) = default;

PairingRegistry::Pairing::~Pairing() = default;

PairingRegistry::Pairing PairingRegistry::Pairing::Create(
    const std::string& client_name) {
  base::Time created_time = base::Time::Now();
  std::string client_id = base::GenerateGUID();
  std::string shared_secret;
  char buffer[kKeySize];
  crypto::RandBytes(buffer, base::size(buffer));
  base::Base64Encode(base::StringPiece(buffer, base::size(buffer)),
                     &shared_secret);
  return Pairing(created_time, client_name, client_id, shared_secret);
}

PairingRegistry::Pairing PairingRegistry::Pairing::CreateFromValue(
    const base::DictionaryValue& pairing) {
  std::string client_name, client_id;
  double created_time_value;
  if (pairing.GetDouble(kCreatedTimeKey, &created_time_value) &&
      pairing.GetString(kClientNameKey, &client_name) &&
      pairing.GetString(kClientIdKey, &client_id)) {
    // The shared secret is optional.
    std::string shared_secret;
    pairing.GetString(kSharedSecretKey, &shared_secret);
    base::Time created_time = base::Time::FromJsTime(created_time_value);
    return Pairing(created_time, client_name, client_id, shared_secret);
  }

  LOG(ERROR) << "Failed to load pairing information: unexpected format.";
  return Pairing();
}

std::unique_ptr<base::DictionaryValue> PairingRegistry::Pairing::ToValue()
    const {
  std::unique_ptr<base::DictionaryValue> pairing(new base::DictionaryValue());
  pairing->SetDouble(kCreatedTimeKey, created_time().ToJsTime());
  pairing->SetString(kClientNameKey, client_name());
  pairing->SetString(kClientIdKey, client_id());
  if (!shared_secret().empty())
    pairing->SetString(kSharedSecretKey, shared_secret());
  return pairing;
}

bool PairingRegistry::Pairing::operator==(const Pairing& other) const {
  return created_time_ == other.created_time_ &&
         client_id_ == other.client_id_ &&
         client_name_ == other.client_name_ &&
         shared_secret_ == other.shared_secret_;
}

bool PairingRegistry::Pairing::is_valid() const {
  // |shared_secret_| is optional. It will be empty on Windows because the
  // privileged registry key can only be read in the elevated host process.
  return !client_id_.empty();
}

PairingRegistry::PairingRegistry(
    scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner,
    std::unique_ptr<Delegate> delegate)
    : caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      delegate_task_runner_(delegate_task_runner),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

PairingRegistry::Pairing PairingRegistry::CreatePairing(
    const std::string& client_name) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  Pairing result = Pairing::Create(client_name);
  AddPairing(result);
  return result;
}

void PairingRegistry::GetPairing(const std::string& client_id,
                                 GetPairingCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  GetPairingCallback wrapped_callback =
      base::BindOnce(&PairingRegistry::InvokeGetPairingCallbackAndScheduleNext,
                     this, std::move(callback));
  ServiceOrQueueRequest(base::BindOnce(&PairingRegistry::DoLoad, this,
                                       client_id, std::move(wrapped_callback)));
}

void PairingRegistry::GetAllPairings(GetAllPairingsCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  GetAllPairingsCallback wrapped_callback = base::BindOnce(
      &PairingRegistry::InvokeGetAllPairingsCallbackAndScheduleNext, this,
      std::move(callback));
  GetAllPairingsCallback sanitize_callback = base::BindOnce(
      &PairingRegistry::SanitizePairings, this, std::move(wrapped_callback));
  ServiceOrQueueRequest(base::BindOnce(&PairingRegistry::DoLoadAll, this,
                                       std::move(sanitize_callback)));
}

void PairingRegistry::DeletePairing(const std::string& client_id,
                                    DoneCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DoneCallback wrapped_callback =
      base::BindOnce(&PairingRegistry::InvokeDoneCallbackAndScheduleNext, this,
                     std::move(callback));
  ServiceOrQueueRequest(base::BindOnce(&PairingRegistry::DoDelete, this,
                                       client_id, std::move(wrapped_callback)));
}

void PairingRegistry::ClearAllPairings(DoneCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DoneCallback wrapped_callback =
      base::BindOnce(&PairingRegistry::InvokeDoneCallbackAndScheduleNext, this,
                     std::move(callback));
  ServiceOrQueueRequest(base::BindOnce(&PairingRegistry::DoDeleteAll, this,
                                       std::move(wrapped_callback)));
}

PairingRegistry::~PairingRegistry() = default;

void PairingRegistry::PostTask(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Location& from_here,
    base::OnceClosure task) {
  task_runner->PostTask(from_here, std::move(task));
}

void PairingRegistry::AddPairing(const Pairing& pairing) {
  DoneCallback wrapped_callback =
      base::BindOnce(&PairingRegistry::InvokeDoneCallbackAndScheduleNext, this,
                     DoneCallback());
  ServiceOrQueueRequest(base::BindOnce(&PairingRegistry::DoSave, this, pairing,
                                       std::move(wrapped_callback)));
}

void PairingRegistry::DoLoadAll(GetAllPairingsCallback callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<base::ListValue> pairings = delegate_->LoadAll();
  PostTask(caller_task_runner_, FROM_HERE,
           base::BindOnce(std::move(callback), std::move(pairings)));
}

void PairingRegistry::DoDeleteAll(DoneCallback callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  bool success = delegate_->DeleteAll();
  PostTask(caller_task_runner_, FROM_HERE,
           base::BindOnce(std::move(callback), success));
}

void PairingRegistry::DoLoad(const std::string& client_id,
                             GetPairingCallback callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  Pairing pairing = delegate_->Load(client_id);
  PostTask(caller_task_runner_, FROM_HERE,
           base::BindOnce(std::move(callback), pairing));
}

void PairingRegistry::DoSave(const Pairing& pairing, DoneCallback callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  bool success = delegate_->Save(pairing);
  PostTask(caller_task_runner_, FROM_HERE,
           base::BindOnce(std::move(callback), success));
}

void PairingRegistry::DoDelete(const std::string& client_id,
                               DoneCallback callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  bool success = delegate_->Delete(client_id);
  PostTask(caller_task_runner_, FROM_HERE,
           base::BindOnce(std::move(callback), success));
}

void PairingRegistry::InvokeDoneCallbackAndScheduleNext(DoneCallback callback,
                                                        bool success) {
  // CreatePairing doesn't have a callback, so the callback can be null.
  if (callback)
    std::move(callback).Run(success);

  pending_requests_.pop();
  ServiceNextRequest();
}

void PairingRegistry::InvokeGetPairingCallbackAndScheduleNext(
    GetPairingCallback callback,
    Pairing pairing) {
  std::move(callback).Run(pairing);
  pending_requests_.pop();
  ServiceNextRequest();
}

void PairingRegistry::InvokeGetAllPairingsCallbackAndScheduleNext(
    GetAllPairingsCallback callback,
    std::unique_ptr<base::ListValue> pairings) {
  std::move(callback).Run(std::move(pairings));
  pending_requests_.pop();
  ServiceNextRequest();
}

void PairingRegistry::SanitizePairings(
    GetAllPairingsCallback callback,
    std::unique_ptr<base::ListValue> pairings) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<base::ListValue> sanitized_pairings(new base::ListValue());
  for (size_t i = 0; i < pairings->GetSize(); ++i) {
    base::DictionaryValue* pairing_json;
    if (!pairings->GetDictionary(i, &pairing_json)) {
      LOG(WARNING) << "A pairing entry is not a dictionary.";
      continue;
    }

    // Parse the pairing data.
    Pairing pairing = Pairing::CreateFromValue(*pairing_json);
    if (!pairing.is_valid()) {
      LOG(WARNING) << "Could not parse a pairing entry.";
      continue;
    }

    // Clear the shared secrect and append the pairing data to the list.
    Pairing sanitized_pairing(
        pairing.created_time(),
        pairing.client_name(),
        pairing.client_id(),
        "");
    sanitized_pairings->Append(sanitized_pairing.ToValue());
  }

  std::move(callback).Run(std::move(sanitized_pairings));
}

void PairingRegistry::ServiceOrQueueRequest(base::OnceClosure request) {
  bool servicing_request = !pending_requests_.empty();
  pending_requests_.emplace(std::move(request));
  if (!servicing_request) {
    ServiceNextRequest();
  }
}

void PairingRegistry::ServiceNextRequest() {
  if (pending_requests_.empty())
    return;

  PostTask(delegate_task_runner_, FROM_HERE,
           std::move(pending_requests_.front()));
}

}  // namespace protocol
}  // namespace remoting
