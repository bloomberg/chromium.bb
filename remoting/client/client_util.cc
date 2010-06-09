// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_util.h"

#include <iostream>

static void SetConsoleEcho(bool on) {
#ifdef WIN32
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if ((hIn == INVALID_HANDLE_VALUE) || (hIn == NULL))
    return;

  DWORD mode;
  if (!GetConsoleMode(hIn, &mode))
    return;

  if (on) {
    mode = mode | ENABLE_ECHO_INPUT;
  } else {
    mode = mode & ~ENABLE_ECHO_INPUT;
  }

  SetConsoleMode(hIn, mode);
#else
  if (on)
    system("stty echo");
  else
    system("stty -echo");
#endif
}

namespace remoting {

// Get host JID from command line arguments, or stdin if not specified.
bool GetLoginInfo(std::string& host_jid,
                  std::string& username,
                  std::string& password) {
  std::cout << "Host JID: ";
  std::cin >> host_jid;
  std::cin.ignore();  // Consume the leftover '\n'

  if (host_jid.find("/chromoting") == std::string::npos) {
    std::cerr << "Error: Expected Host JID in format: <jid>/chromoting<id>"
              << std::endl;
    return false;
  }

  // Get username (JID).
  // Extract default JID from host_jid.
  std::string default_username;
  size_t jid_end = host_jid.find('/');
  if (jid_end != std::string::npos) {
    default_username = host_jid.substr(0, jid_end);
  }
  std::cout << "JID [" << default_username << "]: ";
  getline(std::cin, username);
  if (username.length() == 0) {
    username = default_username;
  }
  if (username.length() == 0) {
    std::cerr << "Error: Expected valid JID username" << std::endl;
    return 1;
  }

  // Get password (with console echo turned off).
  SetConsoleEcho(false);
  std::cout << "Password: ";
  getline(std::cin, password);
  SetConsoleEcho(true);
  std::cout << std::endl;
  return true;
}

}  // namespace remoting
