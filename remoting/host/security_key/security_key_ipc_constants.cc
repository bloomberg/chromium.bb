// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_constants.h"

#include "base/environment.h"
#include "base/lazy_instance.h"

namespace {
base::LazyInstance<std::string> g_security_key_ipc_channel_name =
    LAZY_INSTANCE_INITIALIZER;

const char kSecurityKeyIpcChannelName[] = "security_key_ipc_channel";

}  // namespace

namespace remoting {

extern const char kSecurityKeyConnectionError[] = "ssh_connection_error";

const std::string& GetSecurityKeyIpcChannelName() {
  if (g_security_key_ipc_channel_name.Get().empty()) {
    g_security_key_ipc_channel_name.Get() = kSecurityKeyIpcChannelName;
  }

  return g_security_key_ipc_channel_name.Get();
}

void SetSecurityKeyIpcChannelNameForTest(const std::string& channel_name) {
  g_security_key_ipc_channel_name.Get() = channel_name;
}

std::string GetChannelNamePathPrefixForTest() {
  std::string path_prefix;
#if defined(OS_LINUX)
  path_prefix = "/dev/socket/";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(base::env_vars::kHome, &path_prefix))
    path_prefix += "/";
#endif
  return path_prefix;
}

}  // namespace remoting
