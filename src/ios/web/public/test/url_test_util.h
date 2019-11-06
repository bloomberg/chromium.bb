// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_

#include "base/strings/string16.h"

class GURL;

namespace web {

// Returns a formatted version of |url| that would be used as the fallback title
// for a page with that URL.
base::string16 GetDisplayTitleForUrl(const GURL& url);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_
