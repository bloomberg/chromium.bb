// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_ALLOWLIST_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_ALLOWLIST_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// A short-lived allowlist for bypassing lookalike URL warnings tied to
// web_contents.  Since lookalike URL interstitials only trigger when site
// engagement is low, Chrome does not need to persist interstitial bypasses
// beyond the life of the web_contents-- if the user proceeds to interact with a
// page, site engagement will go up and the allowlist will be bypassed entirely.
class LookalikeUrlAllowlist : public base::SupportsUserData::Data {
 public:
  LookalikeUrlAllowlist();

  ~LookalikeUrlAllowlist() override;

  static LookalikeUrlAllowlist* GetOrCreateAllowlist(
      content::WebContents* web_contents);

  bool IsDomainInList(const std::string& domain);
  void AddDomain(const std::string& domain);

 private:
  std::set<std::string> set_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlAllowlist);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_ALLOWLIST_H_
