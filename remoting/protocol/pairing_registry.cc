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

PairingRegistry::PairingRegistry(scoped_ptr<Delegate> delegate,
                                 const PairedClients& paired_clients)
    : delegate_(delegate.Pass()) {
  DCHECK(delegate_);
  paired_clients_ = paired_clients;
}

PairingRegistry::~PairingRegistry() {
}

const PairingRegistry::Pairing& PairingRegistry::CreatePairing(
    const std::string& client_name) {
  DCHECK(CalledOnValidThread());

  Pairing result;
  result.client_name = client_name;
  result.client_id = base::GenerateGUID();

  // Create a random shared secret to authenticate this client.
  char buffer[kKeySize];
  crypto::RandBytes(buffer, arraysize(buffer));
  result.shared_secret = std::string(buffer, buffer+arraysize(buffer));

  // Save the result via the Delegate and return it to the caller.
  paired_clients_[result.client_id] = result;
  delegate_->Save(paired_clients_);

  return paired_clients_[result.client_id];
}

std::string PairingRegistry::GetSecret(const std::string& client_id) const {
  DCHECK(CalledOnValidThread());

  std::string result;
  PairedClients::const_iterator i = paired_clients_.find(client_id);
  if (i != paired_clients_.end()) {
    result = i->second.shared_secret;
  }
  return result;
}

}  // namespace protocol
}  // namespace remoting
