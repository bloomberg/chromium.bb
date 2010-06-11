// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_util.h"

#include <iostream>

#include "base/logging.h"

namespace remoting {

// Get host JID from command line arguments, or stdin if not specified.
bool GetLoginInfo(int argc, char** argv,
                  std::string* host_jid,
                  std::string* username,
                  std::string* auth_token) {
  bool found_host_jid = false;
  bool found_jid = false;
  bool found_auth_token = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--host_jid") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected Host JID to follow --host_jid option"
                     << std::endl;
      } else {
        found_host_jid = true;
        *host_jid = argv[i];
      }
    } else if (arg == "--jid") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected JID to follow --jid option" << std::endl;
      } else {
        found_jid = true;
        *username = argv[i];
      }
    } else if (arg == "--token") {
      if (++i >= argc) {
        LOG(WARNING) << "Expected Auth token to follow --token option"
                     << std::endl;
      } else {
        found_auth_token = true;
        *auth_token = argv[i];
      }
    } else {
      LOG(WARNING) << "Unrecognized option: " << arg << std::endl;
    }
  }

  if (!found_host_jid) {
    std::cout << "Host JID: ";
    std::cin >> *host_jid;
    std::cin.ignore();  // Consume the leftover '\n'
  }

  // Validate the chromoting host JID.
  if (host_jid->find("/chromoting") == std::string::npos) {
    std::cerr << "Error: Expected Host JID in format: <jid>/chromoting<id>"
              << std::endl;
    return false;
  }

  if (!found_jid) {
    // Get username (JID).
    // Extract default JID from host_jid.
    std::string default_username;
    size_t jid_end = host_jid->find('/');
    if (jid_end != std::string::npos) {
      default_username = host_jid->substr(0, jid_end);
    }
    std::cout << "JID [" << default_username << "]: ";
    getline(std::cin, *username);
    if (username->length() == 0) {
      username->swap(default_username);
    }
    if (username->length() == 0) {
      std::cerr << "Error: Expected valid JID username" << std::endl;
      return false;
    }
  }

  if (!found_auth_token) {
    // Get authentication token.
    std::cout << "Auth token: ";
    getline(std::cin, *auth_token);
    std::cout << std::endl;
  }

  return true;
}

}  // namespace remoting
