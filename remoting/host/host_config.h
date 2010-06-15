// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <string>

#include "base/ref_counted.h"

namespace remoting {

// HostConfig class implements container for all host settings.
class HostConfig : public base::RefCountedThreadSafe<HostConfig> {
 public:
  HostConfig() { }

  // Login used to authenticate in XMPP network.
  const std::string& xmpp_login() const {
    return xmpp_login_;
  }
  void set_xmpp_login(const std::string& xmpp_login) {
    xmpp_login_ = xmpp_login;
  }

  // Auth token used to authenticate in XMPP network.
  const std::string& xmpp_auth_token() const {
    return xmpp_auth_token_;
  }
  void set_xmpp_auth_token(const std::string& xmpp_auth_token) {
    xmpp_auth_token_ = xmpp_auth_token;
  }

  // Unique identifier of the host used to register the host in directory.
  // Normally a random UUID.
  const std::string& host_id() const {
    return host_id_;
  }
  void set_host_id(const std::string& host_id) {
    host_id_ = host_id;
  }

  // Public key used by the host for authentication.
  // TODO(sergeyu): Do we need to use other type to store public key? E.g.
  // DataBuffer? Revisit this when public key generation is implemented.
  const std::string& public_key() const {
    return public_key_;
  }
  void set_public_key(const std::string& public_key) {
    public_key_ = public_key;
  }

  // TODO(sergeyu): Add a property for private key.

 private:
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string host_id_;
  std::string public_key_;

  DISALLOW_COPY_AND_ASSIGN(HostConfig);
};

// Interface for host configuration storage provider.
class HostConfigStorage {
  // Load() and Save() are used to load/save settings to/from permanent
  // storage. For example FileHostConfig stores all settings in a file.
  // Simularly RegistryHostConfig stores settings in windows registry.
  // Both methods return false if operation has failed, true otherwise.
  virtual bool Load(HostConfig* config) = 0;
  virtual bool Save(const HostConfig& config) = 0;

  DISALLOW_COPY_AND_ASSIGN(HostConfigStorage);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
