// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <string.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace url {

Origin::Origin() : unique_(true) {
}

Origin::Origin(const GURL& url) : unique_(true) {
  if (!url.is_valid() || (!url.IsStandard() && !url.SchemeIsBlob()))
    return;

  if (url.SchemeIsFileSystem()) {
    tuple_ = SchemeHostPort(*url.inner_url());
  } else if (url.SchemeIsBlob()) {
    // TODO(mkwst): This relies on the fact that GURL pushes the unparseable
    // bits and pieces of a non-standard scheme into the GURL's path. It seems
    // fairly fragile, so it might be worth teaching GURL about blobs' data in
    // the same way it's been taught about filesystems' inner URLs.
    tuple_ = SchemeHostPort(GURL(url.path()));
  } else {
    tuple_ = SchemeHostPort(url);
  }

  unique_ = tuple_.IsInvalid();
}

Origin::~Origin() {
}

std::string Origin::Serialize() const {
  if (unique())
    return "null";

  if (scheme() == kFileScheme)
    return "file://";

  return tuple_.Serialize();
}

bool Origin::IsSameOriginWith(const Origin& other) const {
  if (unique_ || other.unique_)
    return false;

  return tuple_.Equals(other.tuple_);
}

bool Origin::operator<(const Origin& other) const {
  return tuple_ < other.tuple_;
}

std::ostream& operator<<(std::ostream& out, const url::Origin& origin) {
  return out << origin.Serialize();
}

}  // namespace url
