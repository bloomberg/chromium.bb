// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_
#define WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_
#pragma once

#include "base/basictypes.h"

namespace appcache {

class AppCacheHistograms {
 public:
  enum InitResultType {
    INIT_OK, SQL_DATABASE_ERROR, DISK_CACHE_ERROR,
    NUM_INIT_RESULT_TYPES
  };
  static void CountInitResult(InitResultType init_result);

  enum CheckResponseResultType {
    RESPONSE_OK, MANIFEST_OUT_OF_DATE, RESPONSE_OUT_OF_DATE, ENTRY_NOT_FOUND,
    READ_HEADERS_ERROR, READ_DATA_ERROR, UNEXPECTED_DATA_SIZE, CHECK_CANCELED,
    NUM_CHECK_RESPONSE_RESULT_TYPES
  };
  static void CountCheckResponseResult(CheckResponseResultType result);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppCacheHistograms);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_HISTOGRAMS_H_
