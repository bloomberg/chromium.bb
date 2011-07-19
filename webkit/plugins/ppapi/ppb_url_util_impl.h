// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_UTIL_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_UTIL_IMPL_H_

struct PPB_URLUtil_Dev;

namespace webkit {
namespace ppapi {

class PPB_URLUtil_Impl {
 public:
  static const PPB_URLUtil_Dev* GetInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_UTIL_IMPL_H_
