// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/capabilities.h"

#include <algorithm>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting {

bool HasCapability(const std::string& capabilities, const std::string& key) {
  std::vector<base::StringPiece> caps = base::SplitStringPiece(
      capabilities, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::find(caps.begin(), caps.end(), base::StringPiece(key)) !=
         caps.end();
}

std::string IntersectCapabilities(const std::string& client_capabilities,
                                  const std::string& host_capabilities) {
  std::vector<std::string> client_caps = base::SplitString(
      client_capabilities, " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::sort(client_caps.begin(), client_caps.end());

  std::vector<std::string> host_caps = base::SplitString(
        host_capabilities, " ", base::KEEP_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
  std::sort(host_caps.begin(), host_caps.end());

  std::vector<std::string> result =
      base::STLSetIntersection<std::vector<std::string> >(
          client_caps, host_caps);

  return base::JoinString(result, " ");
}

}  // namespace remoting
