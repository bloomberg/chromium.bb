// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_util.h"

#include <iostream>

namespace remoting {

// Get host JID from command line arguments, or stdin if not specified.
bool GetLoginInfo(std::string* host_jid,
                  std::string* username,
                  std::string* auth_token) {
  std::cout << "Host JID: ";
  std::cin >> *host_jid;
  std::cin.ignore();  // Consume the leftover '\n'

  if (host_jid->find("/chromoting") == std::string::npos) {
    std::cerr << "Error: Expected Host JID in format: <jid>/chromoting<id>"
              << std::endl;
    return false;
  }

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
    return 1;
  }

  // Get authenication token (with console echo turned off).
  std::cout << "Auth token: ";
  getline(std::cin, *auth_token);
  std::cout << std::endl;
  return true;
}

}  // namespace remoting
