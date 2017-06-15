// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCachePolicy_h
#define WebCachePolicy_h

namespace blink {

enum class WebCachePolicy : uint8_t {
  kUseProtocolCachePolicy,   // normal load
  kValidatingCacheData,      // reload
  kBypassingCache,           // end-to-end reload
  kReturnCacheDataElseLoad,  // back/forward or encoding change - allow stale
                             // data
  kReturnCacheDataDontLoad,  // results of a post - allow stale data and only
                             // use cache
  kReturnCacheDataIfValid,   // for cache-aware loading - disallow stale data
  kBypassCacheLoadOnlyFromCache,  // for cache-only load when disable cache
                                  // is enabled. Results in a network error.
};

}  // namespace blink

#endif  // WebCachePolicy_h
