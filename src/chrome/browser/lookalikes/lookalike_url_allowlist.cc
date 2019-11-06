// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_allowlist.h"

#include <string>

#include "content/public/browser/web_contents.h"

// This bit of chaos ensures that kAllowlistKey is an arbitrary but
// unique-in-the-process value (namely, its own memory address) without casts.
const void* const kAllowlistKey = &kAllowlistKey;

LookalikeUrlAllowlist::LookalikeUrlAllowlist() {}

LookalikeUrlAllowlist::~LookalikeUrlAllowlist() {}

// static
LookalikeUrlAllowlist* LookalikeUrlAllowlist::GetOrCreateAllowlist(
    content::WebContents* web_contents) {
  LookalikeUrlAllowlist* allowlist = static_cast<LookalikeUrlAllowlist*>(
      web_contents->GetUserData(kAllowlistKey));
  if (!allowlist) {
    allowlist = new LookalikeUrlAllowlist;
    web_contents->SetUserData(kAllowlistKey, base::WrapUnique(allowlist));
  }
  return allowlist;
}

bool LookalikeUrlAllowlist::IsDomainInList(const std::string& domain) {
  return set_.count(domain) > 0;
}

void LookalikeUrlAllowlist::AddDomain(const std::string& domain) {
  set_.insert(domain);
}
