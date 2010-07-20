// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONFIG_H_
#define REMOTING_CLIENT_CLIENT_CONFIG_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

class ClientConfig {
 public:
  ClientConfig() { }

  const std::string host_jid() { return host_jid_; }
  const std::string username() { return username_; }
  const std::string auth_token() { return auth_token_; }

  void set_host_jid(std::string val) { host_jid_ = val; }
  void set_username(std::string val) { username_ = val; }
  void set_auth_token(std::string val) { auth_token_ = val; }

 private:
  std::string host_jid_;
  std::string username_;
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(ClientConfig);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONFIG_H_
