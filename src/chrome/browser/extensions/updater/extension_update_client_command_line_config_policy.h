// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_

#include "base/macros.h"
#include "components/update_client/command_line_config_policy.h"

namespace base {
class CommandLine;
}

namespace extensions {

// This class implements the command line policy for the extension updater
// using update client.
class ExtensionUpdateClientCommandLineConfigPolicy final
    : public update_client::CommandLineConfigPolicy {
 public:
  explicit ExtensionUpdateClientCommandLineConfigPolicy(
      const base::CommandLine* cmdline);

  // update_client::CommandLineConfigPolicy overrides.
  bool TestRequest() const override;

 private:
  bool test_request_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdateClientCommandLineConfigPolicy);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
