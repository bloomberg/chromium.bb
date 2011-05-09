// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_util.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "remoting/client/client_config.h"

using std::string;
using std::vector;

namespace remoting {

// Get host JID from command line arguments, or stdin if not specified.
bool GetLoginInfoFromArgs(int argc, char** argv, ClientConfig* config) {
  bool found_host_jid = false;
  bool found_jid = false;
  bool found_auth_token = false;
  string host_jid;
  string username;
  string auth_token;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--host_jid") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected Host JID to follow --host_jid option";
      } else {
        found_host_jid = true;
        host_jid = argv[i];
      }
    } else if (arg == "--jid") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected JID to follow --jid option";
      } else {
        found_jid = true;
        username = argv[i];
      }
    } else if (arg == "--token") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected Auth token to follow --token option";
      } else {
        found_auth_token = true;
        auth_token = argv[i];
      }
    } else {
      LOG(WARNING) << "Unrecognized option: " << arg;
    }
  }

  if (!found_host_jid)
    return false;

  // Validate the chromoting host JID.
  if ((host_jid.find("/chromoting") == std::string::npos) || !found_jid ||
      !found_auth_token)
    return false;

  NOTIMPLEMENTED() << "Nonce ignored.";

  config->host_jid = host_jid;
  config->username = username;
  config->auth_token = auth_token;
  return true;
}

}  // namespace remoting
