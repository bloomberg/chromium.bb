// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace remoting {

// Following constants define names for configuration parameters.

// Status of the host, whether it is enabled or disabled.
extern const char kHostEnabledConfigPath[];
// Login used to authenticate in XMPP network.
extern const char kXmppLoginConfigPath[];
// Auth token used to authenticate to XMPP network.
extern const char kXmppAuthTokenConfigPath[];
// Auth service used to authenticate to XMPP network.
extern const char kXmppAuthServiceConfigPath[];
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
extern const char kHostIdConfigPath[];
// Readable host name.
extern const char kHostNameConfigPath[];
// Private keys used for host authentication.
extern const char kPrivateKeyConfigPath[];

// HostConfig interace provides read-only access to host configuration.
class HostConfig : public base::RefCountedThreadSafe<HostConfig> {
 public:
  HostConfig() {}
  virtual ~HostConfig() {}

  virtual bool GetString(const std::string& path, std::string* out_value) = 0;
  virtual bool GetBoolean(const std::string& path, bool* out_value) = 0;

  DISALLOW_COPY_AND_ASSIGN(HostConfig);
};

// MutableHostConfig extends HostConfig for mutability.
class MutableHostConfig : public HostConfig {
 public:
  MutableHostConfig() {}

  // SetString() updates specified config value. Save() must be called to save
  // the changes on the disk.
  virtual void SetString(const std::string& path,
                         const std::string& in_value) = 0;
  virtual void SetBoolean(const std::string& path, bool in_value) = 0;

  // Save's changes.
  virtual void Save() = 0;

  DISALLOW_COPY_AND_ASSIGN(MutableHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
