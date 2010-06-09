// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_UTIL_H_
#define REMOTING_CLIENT_CLIENT_UTIL_H_

#include <string>

namespace remoting {

// Get the login info from the console and writes into |host_jid|, |username|
// and |password|. Return true if successful.
bool GetLoginInfo(std::string& host_jid, std::string& username,
                  std::string& password);

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_UTIL_H_
