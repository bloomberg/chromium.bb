// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
        LOG(WARNING) << "Expected Host JID to follow --host_jid option"
                     << std::endl;
      } else {
        found_host_jid = true;
        host_jid = argv[i];
      }
    } else if (arg == "--jid") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected JID to follow --jid option" << std::endl;
      } else {
        found_jid = true;
        username = argv[i];
      }
    } else if (arg == "--token") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected Auth token to follow --token option"
                     << std::endl;
      } else {
        found_auth_token = true;
        auth_token = argv[i];
      }
    } else {
      LOG(WARNING) << "Unrecognized option: " << arg << std::endl;
    }
  }

  if (!found_host_jid) {
    return false;
  }

  // Validate the chromoting host JID.
  if (host_jid.find("/chromoting") == std::string::npos) {
    return false;
  }

  if (!found_jid) {
    return false;
  }

  if (!found_auth_token) {
    return false;
  }

  config->set_host_jid(host_jid);
  config->set_username(username);
  config->set_auth_token(auth_token);
  return true;
}

// Get host JID from command line arguments, or stdin if not specified.
bool GetLoginInfoFromUrlParams(const std::string& url, ClientConfig* config) {
  // TODO(ajwong): We should use GURL or something.  Don't parse this by hand!

  // The Url should be of the form:
  //
  //   chrome://remoting?user=<userid>&auth=<authtoken>&jid=<hostjid>
  //
  vector<string> parts;
  SplitString(url, '&', &parts);
  if (parts.size() != 3) {
    return false;
  }

  size_t pos = parts[0].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  std::string username = parts[0].substr(pos + 1);

  pos = parts[1].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  std::string auth_token = parts[1].substr(pos + 1);

  pos = parts[2].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  std::string host_jid = parts[2].substr(pos + 1);

  config->set_host_jid(host_jid);
  config->set_username(username);
  config->set_auth_token(auth_token);
  return true;
}

}  // namespace remoting
