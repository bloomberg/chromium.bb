// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_KEY_PAIR_H_
#define REMOTING_HOST_HOST_KEY_PAIR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace base

namespace remoting {

class HostConfig;
class MutableHostConfig;

class HostKeyPair {
 public:
  HostKeyPair();
  ~HostKeyPair();

  void Generate();
  bool LoadFromString(const std::string& key_base64);
  bool Load(const HostConfig& host_config);
  void Save(MutableHostConfig* host_config);

  crypto::RSAPrivateKey* private_key() { return key_.get(); }

  std::string GetAsString() const;
  std::string GetPublicKey() const;
  std::string GetSignature(const std::string& message) const;

  // Make a new copy of private key. Caller will own the generated private key.
  crypto::RSAPrivateKey* CopyPrivateKey() const;

  // Generates self-signed certificate using the key pair. Returns empty string
  // if cert generation fails (e.g. it may happen when the system clock is off).
  std::string GenerateCertificate() const;

 private:
  scoped_ptr<crypto::RSAPrivateKey> key_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_KEY_PAIR_H_
