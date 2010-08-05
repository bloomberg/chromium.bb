// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_

typedef struct _ppb_UrlUtil PPB_UrlUtil;

namespace pepper {

const PPB_UrlUtil* GetUrlUtilInterface();

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_UTIL_H_
