// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_TOP_HOST_PROVIDER_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_TOP_HOST_PROVIDER_H_

#include <string>
#include <vector>

namespace previews {

// A class to handle querying for the top hosts for a user.
class PreviewsTopHostProvider {
 public:
  // Returns a vector of at most |max_sites| top hosts, the order of hosts is
  // not guaranteed.
  virtual std::vector<std::string> GetTopHosts(size_t max_sites) const = 0;

 protected:
  PreviewsTopHostProvider() {}
  virtual ~PreviewsTopHostProvider() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_TOP_HOST_PROVIDER_H_
