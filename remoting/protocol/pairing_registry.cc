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

// How many bytes of random data to use for the client id and shared secret.
const int kKeySize = 16;

PairingRegistry::PairingRegistry(scoped_ptr<Delegate> delegate)
    : delegate_(delegate.Pass()) {
  DCHECK(delegate_);
}

PairingRegistry::~PairingRegistry() {
}

PairingRegistry::Pairing PairingRegistry::CreatePairing(
    const std::string& client_name) {
  DCHECK(CalledOnValidThread());

  Pairing result;
  result.client_name = client_name;
  result.client_id = base::GenerateGUID();

  // Create a random shared secret to authenticate this client.
  char buffer[kKeySize];
  crypto::RandBytes(buffer, arraysize(buffer));
  if (!base::Base64Encode(base::StringPiece(buffer, arraysize(buffer)),
                          &result.shared_secret)) {
    LOG(FATAL) << "Base64Encode failed.";
  }

  // Save the result via the Delegate and return it to the caller.
  delegate_->AddPairing(result);
  return result;
}

void PairingRegistry::GetPairing(const std::string& client_id,
                                 const GetPairingCallback& callback) {
  DCHECK(CalledOnValidThread());
  delegate_->GetPairing(client_id, callback);
}

void NotImplementedPairingRegistryDelegate::AddPairing(
    const PairingRegistry::Pairing& new_paired_client) {
  NOTIMPLEMENTED();
}

void NotImplementedPairingRegistryDelegate::GetPairing(
    const std::string& client_id,
    const PairingRegistry::GetPairingCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(PairingRegistry::Pairing());
}


}  // namespace protocol
}  // namespace remoting
