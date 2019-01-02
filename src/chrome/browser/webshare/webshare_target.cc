// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/webshare_target.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"

WebShareTarget::WebShareTarget(const GURL& manifest_url,
                               const std::string& name,
                               const GURL& action,
                               const std::string& text,
                               const std::string& title,
                               const std::string& url)
    : manifest_url_(manifest_url),
      name_(name),
      action_(action),
      text_(text),
      title_(title),
      url_(url) {}

WebShareTarget::~WebShareTarget() {}

WebShareTarget::WebShareTarget(WebShareTarget&& other) = default;

bool WebShareTarget::operator==(const WebShareTarget& other) const {
  return std::tie(manifest_url_, name_, action_, text_, title_, url_) ==
         std::tie(other.manifest_url_, other.name_, other.action_, text_,
                  title_, url_);
}

std::ostream& operator<<(std::ostream& out, const WebShareTarget& target) {
  return out << "WebShareTarget(GURL(" << target.manifest_url().spec() << "), "
             << target.name() << ", " << target.action() << ", "
             << target.text() << ", " << target.title() << ", " << target.url()
             << ")";
}
