// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jid_util.h"

#include <stddef.h>

#include "base/strings/string_util.h"

namespace remoting {

namespace {

constexpr char kFtlResourcePrefix[] = "chromoting_ftl_";
constexpr char kGmailDomain[] = "gmail.com";
constexpr char kGooglemailDomain[] = "googlemail.com";

}  // namespace

std::string NormalizeJid(const std::string& jid) {
  std::string bare_jid;
  std::string resource;
  if (SplitJidResource(jid, &bare_jid, &resource)) {
    std::string normalized_bare_jid = resource.find(kFtlResourcePrefix) == 0
                                          ? GetCanonicalEmail(bare_jid)
                                          : base::ToLowerASCII(bare_jid);
    return normalized_bare_jid + "/" + resource;
  }
  return base::ToLowerASCII(bare_jid);
}

std::string GetCanonicalEmail(std::string email) {
  DCHECK(email.find('/') == std::string::npos)
      << "You seemed to pass in a full JID. You should only pass in an email "
      << "address.";
  email = base::ToLowerASCII(email);
  base::TrimString(email, base::kWhitespaceASCII, &email);

  size_t at_index = email.find('@');
  if (at_index == std::string::npos) {
    LOG(ERROR) << "Unexpected email address. Character '@' is missing.";
    return email;
  }
  std::string username = email.substr(0, at_index);
  std::string domain = email.substr(at_index + 1);

  if (domain == kGmailDomain || domain == kGooglemailDomain) {
    // GMail/GoogleMail domains ignore dots, whereas other domains may not.
    base::RemoveChars(username, ".", &username);
    return username + '@' + kGmailDomain;
  }

  return email;
}

bool SplitJidResource(const std::string& full_jid,
                      std::string* bare_jid,
                      std::string* resource) {
  size_t slash_index = full_jid.find('/');
  if (slash_index == std::string::npos) {
    if (bare_jid) {
      *bare_jid = full_jid;
    }
    if (resource) {
      resource->clear();
    }
    return false;
  }

  if (bare_jid) {
    *bare_jid = full_jid.substr(0, slash_index);
  }
  if (resource) {
    *resource = full_jid.substr(slash_index + 1);
  }
  return true;
}

}  // namespace remoting
