// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <string>

#include "base/ref_counted.h"

class Task;

namespace remoting {

// Following constants define names for configuration parameters.

// Login used to authenticate in XMPP network.
extern const wchar_t* kXmppLoginConfigPath;
// Auth token used to authenticate in XMPP network.
extern const wchar_t* kXmppAuthTokenConfigPath;
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
extern const wchar_t* kHostIdConfigPath;
// Readable host name.
extern const wchar_t* kHostNameConfigPath;
// Private keys used for host authentication.
extern const wchar_t* kPrivateKeyConfigPath;

// HostConfig interace provides read-only access to host configuration.
class HostConfig : public base::RefCountedThreadSafe<HostConfig> {
 public:
  HostConfig() { };
  virtual ~HostConfig() { }

  virtual bool GetString(const std::wstring& path,
                         std::wstring* out_value) = 0;
  virtual bool GetString(const std::wstring& path,
                         std::string* out_value) = 0;

  DISALLOW_COPY_AND_ASSIGN(HostConfig);
};

// MutableHostConfig extends HostConfig for mutability.
class MutableHostConfig : public HostConfig {
 public:
  MutableHostConfig() { };

  // Update() must be used to update config values.
  // It acquires lock, calls the specified task, releases the lock and
  // then schedules the config to be written to storage.
  virtual void Update(Task* task) = 0;

  // SetString() updates specified config value. This methods must only
  // be called from task specified in Update().
  virtual void SetString(const std::wstring& path,
                         const std::wstring& in_value) = 0;
  virtual void SetString(const std::wstring& path,
                         const std::string& in_value) = 0;

  DISALLOW_COPY_AND_ASSIGN(MutableHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
