// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_KEY_PAIR_H_
#define REMOTING_HOST_HOST_KEY_PAIR_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"

namespace base {
class RSAPrivateKey;
}  // namespace base

namespace net {
class X509Certificate;
}  // namespace net

namespace remoting {

class HostConfig;
class MutableHostConfig;

class HostKeyPair {
 public:
  HostKeyPair();
  ~HostKeyPair();

  void Generate();
  bool LoadFromString(const std::string& key_base64);
  bool Load(HostConfig* host_config);
  void Save(MutableHostConfig* host_config);

  std::string GetPublicKey() const;
  std::string GetSignature(const std::string& message) const;

  // Make a new copy of private key. Caller will own the generated private key.
  base::RSAPrivateKey* CopyPrivateKey() const;
  net::X509Certificate* GenerateCertificate() const;

 private:
  scoped_ptr<base::RSAPrivateKey> key_;
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::HostKeyPair);

#endif  // REMOTING_HOST_HOST_KEY_PAIR_H_
