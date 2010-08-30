// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_

struct PPB_UrlUtil_Dev;

namespace pepper {

class UrlUtil {
 public:
  static const PPB_UrlUtil_Dev* GetInterface();
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_
