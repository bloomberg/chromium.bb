// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_PLUGINS_PPAPI_PPAPI_UTILS_H_
#define WEBKIT_COMMON_PLUGINS_PPAPI_PPAPI_UTILS_H_

namespace webkit {
namespace ppapi {

// Returns true if the interface name passed in is supported by the
// browser.
bool IsSupportedPepperInterface(const char* name);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_COMMON_PLUGINS_PPAPI_PPAPI_UTILS_H_


