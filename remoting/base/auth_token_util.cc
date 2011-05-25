// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auth_token_util.h"
#include "remoting/base/constants.h"

namespace remoting {

void ParseAuthTokenWithService(const std::string& auth_service_with_token,
                               std::string* auth_token,
                               std::string* auth_service) {
  size_t delimiter_pos = auth_service_with_token.find(':');
  if (delimiter_pos == std::string::npos) {
    // Legacy case: there is no delimiter. Assume the whole string is the
    // auth_token, and that we're using the default service.
    //
    // TODO(ajwong): Remove this defaulting once all webclients are migrated.
    // BUG:83897
    auth_token->assign(auth_service_with_token);
    auth_service->assign(kChromotingTokenDefaultServiceName);
  } else {
    auth_service->assign(auth_service_with_token.substr(0, delimiter_pos));

    // Make sure there is *something* after the delimiter before doing substr.
    if (delimiter_pos < auth_service_with_token.size()) {
      auth_token->assign(auth_service_with_token.substr(delimiter_pos + 1));
    } else {
      auth_token->clear();
    }
  }
}

}  // namespace remoting
