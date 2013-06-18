// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "crypto/random.h"

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
  delegate_->AddPairing(result, AddPairingCallback());
  return result;
}

void PairingRegistry::GetPairing(const std::string& client_id,
                                 const GetPairingCallback& callback) {
  DCHECK(CalledOnValidThread());
  delegate_->GetPairing(client_id, callback);
}

void NotImplementedPairingRegistryDelegate::AddPairing(
    const PairingRegistry::Pairing& new_paired_client,
    const PairingRegistry::AddPairingCallback& callback) {
  NOTIMPLEMENTED();
  if (!callback.is_null()) {
    callback.Run(false);
  }
}

void NotImplementedPairingRegistryDelegate::GetPairing(
    const std::string& client_id,
    const PairingRegistry::GetPairingCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(PairingRegistry::Pairing());
}

}  // namespace protocol
}  // namespace remoting
