// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

namespace remoting {

// These constants are broken out of host_config.cc to allow them to be pulled
// in by the Mac prefpane implementation without introducing unused dependencies
// on class from src/base.
const char kHostEnabledConfigPath[] = "enabled";
const char kHostOwnerConfigPath[] = "host_owner";
const char kHostOwnerEmailConfigPath[] = "host_owner_email";
const char kXmppLoginConfigPath[] = "xmpp_login";
const char kOAuthRefreshTokenConfigPath[] = "oauth_refresh_token";
const char kHostIdConfigPath[] = "host_id";
const char kHostNameConfigPath[] = "host_name";
const char kHostSecretHashConfigPath[] = "host_secret_hash";
const char kPrivateKeyConfigPath[] = "private_key";
const char kUsageStatsConsentConfigPath[] = "usage_stats_consent";
const char kEnableVp9ConfigPath[] = "enable_vp9";
const char kFrameRecorderBufferKbConfigPath[] = "frame-recorder-buffer-kb";

}  // namespace remoting
