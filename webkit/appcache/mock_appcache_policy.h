// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_POLICY_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_POLICY_H_

#include "base/compiler_specific.h"
#include "googleurl/src/gurl.h"
#include "webkit/appcache/appcache_policy.h"

namespace appcache {

class MockAppCachePolicy : public AppCachePolicy {
 public:
  MockAppCachePolicy();
  virtual ~MockAppCachePolicy();

  virtual bool CanLoadAppCache(const GURL& manifest_url,
                               const GURL& first_party) OVERRIDE;
  virtual bool CanCreateAppCache(const GURL& manifest_url,
                                 const GURL& first_party) OVERRIDE;

  bool can_load_return_value_;
  bool can_create_return_value_;
  GURL requested_manifest_url_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_POLICY_H_
