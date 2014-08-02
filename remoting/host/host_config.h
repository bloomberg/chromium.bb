// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <string>

#include "base/basictypes.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

// Following constants define names for configuration parameters.

// Status of the host, whether it is enabled or disabled.
extern const char kHostEnabledConfigPath[];
// Google account of the owner of this host.
extern const char kHostOwnerConfigPath[];
// Login used to authenticate in XMPP network.
extern const char kXmppLoginConfigPath[];
// Auth token used to authenticate to XMPP network.
extern const char kXmppAuthTokenConfigPath[];
// OAuth refresh token used to fetch an access token for the XMPP network.
extern const char kOAuthRefreshTokenConfigPath[];
// Auth service used to authenticate to XMPP network.
extern const char kXmppAuthServiceConfigPath[];
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
extern const char kHostIdConfigPath[];
// Readable host name.
extern const char kHostNameConfigPath[];
// Hash of the host secret used for authentication.
extern const char kHostSecretHashConfigPath[];
// Private keys used for host authentication.
extern const char kPrivateKeyConfigPath[];
// Whether consent is given for usage stats reporting.
extern const char kUsageStatsConsentConfigPath[];
// Whether to offer VP9 encoding to clients.
extern const char kEnableVp9ConfigPath[];
// Number of Kibibytes of frame data to allow each client to record.
extern const char kFrameRecorderBufferKbConfigPath[];

// HostConfig interace provides read-only access to host configuration.
class HostConfig {
 public:
  HostConfig() {}
  virtual ~HostConfig() {}

  virtual bool GetString(const std::string& path,
                         std::string* out_value) const = 0;
  virtual bool GetBoolean(const std::string& path, bool* out_value) const = 0;

 private:
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

  // Copy configuration from specified |dictionary|. Returns false if the
  // |dictionary| contains some values that cannot be saved in the config. In
  // that case, all other values are still copied.
  virtual bool CopyFrom(const base::DictionaryValue* dictionary) = 0;

  // Saves changes.
  virtual bool Save() = 0;

  DISALLOW_COPY_AND_ASSIGN(MutableHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
