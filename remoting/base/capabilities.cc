// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/capabilities.h"

#include <algorithm>
#include <vector>

#include "base/string_util.h"

namespace remoting {

bool HasCapability(const std::string& capabilities, const std::string& key) {
  std::vector<std::string> caps;
  Tokenize(capabilities, " ", &caps);
  return std::find(caps.begin(), caps.end(), key) != caps.end();
}

std::string IntersectCapabilities(const std::string& client_capabilities,
                                  const std::string& host_capabilities) {
  std::vector<std::string> client_caps;
  Tokenize(client_capabilities, " ", &client_caps);
  std::sort(client_caps.begin(), client_caps.end());

  std::vector<std::string> host_caps;
  Tokenize(host_capabilities, " ", &host_caps);
  std::sort(host_caps.begin(), host_caps.end());

  std::vector<std::string> result(std::min(client_caps.size(),
                                           host_caps.size()));
  std::vector<std::string>::iterator end =
      std::set_intersection(client_caps.begin(),
                            client_caps.end(),
                            host_caps.begin(),
                            host_caps.end(),
                            result.begin());
  if (end != result.end())
    result.erase(end, result.end());

  return JoinString(result, " ");
}

}  // namespace remoting
