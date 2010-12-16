// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_ERROR_UTIL_H_
#define WEBKIT_PLUGINS_PPAPI_ERROR_UTIL_H_

#include "base/platform_file.h"

namespace webkit {
namespace ppapi {

int PlatformFileErrorToPepperError(base::PlatformFileError error_code);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_ERROR_UTIL_H_
