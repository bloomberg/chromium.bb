// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_AUTH_TOKEN_UTIL_H_
#define REMOTING_BASE_AUTH_TOKEN_UTIL_H_

#include <string>

namespace remoting {

// Given a string of the form "auth_service:auth_token" parses it into its
// component pieces.
void ParseAuthTokenWithService(const std::string& auth_service_with_token,
                               std::string* auth_token,
                               std::string* auth_service);

}  // namespace remoting

#endif  // REMOTING_BASE_AUTH_TOKEN_UTIL_H_
