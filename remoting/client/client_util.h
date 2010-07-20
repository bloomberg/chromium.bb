// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_UTIL_H_
#define REMOTING_CLIENT_CLIENT_UTIL_H_

#include <string>

namespace remoting {

class ClientConfig;

// Get the login info from the cmdline args (or request from the console if
// not present) and write values into |config|.
// Return true if successful.
bool GetLoginInfoFromArgs(int argc, char** argv, ClientConfig* config);

// Get the login info from the URL params and write values into |config|.
// Return true if successful.
bool GetLoginInfoFromUrlParams(const std::string& url, ClientConfig* config);

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_UTIL_H_
