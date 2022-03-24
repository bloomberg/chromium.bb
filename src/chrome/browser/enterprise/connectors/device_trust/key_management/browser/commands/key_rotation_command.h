// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_H_

#include <string>

#include "base/callback.h"

namespace enterprise_connectors {

// Interface for classes that handle kicking-off device trust key rotation
// commands. There is an implementation for each platform.
class KeyRotationCommand {
 public:
  // Completion status of the rotate command, passed as an argument to the
  // trigger callback.
  enum class Status {
    SUCCEEDED,
    FAILED,
    TIMED_OUT,
  };

  // Trigger completion callback.  The single argument is the status of the
  // rotate command.
  using Callback = base::OnceCallback<void(Status)>;

  virtual ~KeyRotationCommand() = default;

  struct Params {
    std::string dm_token;
    std::string dm_server_url;
    std::string nonce;
  };

  // Kicks off a platform-specific key rotation command using the given
  // `params`.  Invokes `callback` upon completion.
  virtual void Trigger(const Params& params, Callback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_H_
