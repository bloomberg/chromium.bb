// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_URL_MAPPER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_URL_MAPPER_H_

#include "url/gurl.h"

namespace favicon {

// Interface for the translation of a page URL to the URL of the favicon the
// browser associated to this page.
class FaviconUrlMapper {
 public:
  virtual ~FaviconUrlMapper() = default;
  // Returns the corresponding icon url if such exists, otherwise returns the
  // empty GURL.
  virtual GURL GetIconUrlForPageUrl(const GURL& page_url) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_URL_MAPPER_H_
