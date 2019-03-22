// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_
#define CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_

#include <string>

#include "url/gurl.h"

// Represents a Web Share Target and its attributes. The attributes are usually
// retrieved from the share_target field in the site's manifest.
class WebShareTarget {
 public:
  WebShareTarget(const GURL& manifest_url,
                 const std::string& name,
                 const GURL& action,
                 const std::string& text,
                 const std::string& title,
                 const std::string& url);
  ~WebShareTarget();

  // Move constructor
  WebShareTarget(WebShareTarget&& other);

  // Move assigment
  WebShareTarget& operator=(WebShareTarget&& other) = default;

  const std::string& name() const { return name_; }
  const GURL& manifest_url() const { return manifest_url_; }
  // The action URL to append query parameters to.
  const GURL& action() const { return action_; }

  // Parameters
  const std::string& text() const { return text_; }
  const std::string& title() const { return title_; }
  const std::string& url() const { return url_; }

  bool operator==(const WebShareTarget& other) const;

 private:
  GURL manifest_url_;
  std::string name_;
  GURL action_;
  std::string text_;
  std::string title_;
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(WebShareTarget);
};

// Used by gtest to print a readable output on test failures.
std::ostream& operator<<(std::ostream& out, const WebShareTarget& target);

#endif  // CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_
