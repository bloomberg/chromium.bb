// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/access_verifier.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "remoting/host/host_config.h"

namespace remoting {

AccessVerifier::AccessVerifier()
    : initialized_(false) {
}

bool AccessVerifier::Init(HostConfig* config) {
  std::string host_jid;

  if (!config->GetString(kXmppLoginConfigPath, &host_jid) ||
      host_jid.empty()) {
    LOG(ERROR) << "XMPP credentials are not defined in the config.";
    return false;
  }

  host_jid_prefix_ = host_jid + '/';
  initialized_ = true;

  return true;
}

bool AccessVerifier::VerifyPermissions(const std::string& client_jid) {
  CHECK(initialized_);
  // Check that the client has the same bare jid as the host, i.e.
  // client's full jid starts with host's bare jid.
  return StartsWithASCII(client_jid, host_jid_prefix_, true);
}

}  // namespace remoting
