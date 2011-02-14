// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/access_verifier.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "remoting/host/host_config.h"
#include "remoting/proto/auth.pb.h"

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

bool AccessVerifier::VerifyPermissions(
    const std::string& client_jid,
    const std::string& encoded_access_token) {
  CHECK(initialized_);

  // Reject incoming connection if the client's jid is not an ASCII string.
  if (!IsStringASCII(client_jid)) {
    LOG(ERROR) << "Rejecting incoming connection from " << client_jid;
    return false;
  }

  // Check that the client has the same bare jid as the host, i.e.
  // client's full JID starts with host's bare jid. Comparison is case
  // insensitive.
  if (!StartsWithASCII(client_jid, host_jid_prefix_, false)) {
    LOG(ERROR) << "Rejecting incoming connection from " << client_jid;
    return false;
  }

  // Decode the auth token.
  protocol::ClientAuthToken client_token;
  if (!DecodeClientAuthToken(encoded_access_token, &client_token)) {
    return false;
  }

  // Kick off directory access permissions.
  // TODO(ajwong): Actually implement this.
  return true;
}

bool AccessVerifier::DecodeClientAuthToken(
    const std::string& encoded_client_token,
    protocol::ClientAuthToken* client_token) {
  // TODO(ajwong): Implement this.
  NOTIMPLEMENTED();
  return true;
}

}  // namespace remoting
