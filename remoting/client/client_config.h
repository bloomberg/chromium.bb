// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONFIG_H_
#define REMOTING_CLIENT_CLIENT_CONFIG_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

struct ClientConfig {
  ClientConfig();
  ~ClientConfig();

  std::string host_jid;
  std::string username;
  std::string auth_token;
  std::string nonce;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONFIG_H_
