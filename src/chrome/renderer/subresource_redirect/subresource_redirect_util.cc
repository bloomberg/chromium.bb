// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"

#include <string>

#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/base32/base32.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace subresource_redirect {

GURL GetSubresourceURLForURL(const GURL& original_url) {
  DCHECK(original_url.is_valid());

  std::string fragment;
  if (original_url.has_ref()) {
    fragment = "#" + original_url.ref();
  }

  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::NumberToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  GURL subresource_host("https://litepages.googlezip.net/");
  // TODO(harrisonsean): Add experiment (x=) param.
  GURL compressed_url(
      subresource_host.scheme() + "://" + origin_hash + "." +
      subresource_host.host() +
      (subresource_host.has_port() ? (":" + subresource_host.port()) : "") +
      "/sr?u=" +
      // Strip out the fragment so that it is not sent to the server.
      net::EscapeQueryParamValue(original_url.GetAsReferrer().spec(),
                                 true /* use_plus */) +
      "&t=image" + fragment);

  DCHECK(compressed_url.is_valid());
  DCHECK_EQ(subresource_host.scheme(), compressed_url.scheme());
  return compressed_url;
}

}  // namespace subresource_redirect