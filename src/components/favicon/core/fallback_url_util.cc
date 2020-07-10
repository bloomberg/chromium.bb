// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_url_util.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {
const char* kFallbackIconTextForIP = "IP";
}  // namespace

namespace favicon {

base::string16 GetFallbackIconText(const GURL& url) {
  if (url.is_empty())
    return base::string16();
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {  // E.g., http://localhost/ or http://192.168.0.1/
    if (url.HostIsIPAddress())
      return base::ASCIIToUTF16(kFallbackIconTextForIP);
    domain = url.host();
  }
  if (domain.empty())
    return base::string16();
  // TODO(huangs): Handle non-ASCII ("xn--") domain names.
  return base::i18n::ToUpper(base::ASCIIToUTF16(domain.substr(0, 1)));
}

}  // namespace favicon
