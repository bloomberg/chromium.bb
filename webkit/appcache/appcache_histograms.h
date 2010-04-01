// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_
#define WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_

namespace appcache {

class AppCacheHistograms {
 public:
  enum InitResultType {
    INIT_OK, SQL_DATABASE_ERROR, DISK_CACHE_ERROR,
    NUM_INIT_RESULT_TYPES
  };
  static void CountInitResult(InitResultType init_result);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_
