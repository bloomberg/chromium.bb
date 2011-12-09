// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_FILE_TYPE_CONVERSIONS_H_
#define WEBKIT_PLUGINS_PPAPI_FILE_TYPE_CONVERSIONS_H_

#include "base/platform_file.h"
#include "ppapi/c/pp_stdint.h"

namespace webkit {
namespace ppapi {

int PlatformFileErrorToPepperError(base::PlatformFileError error_code);

// Converts a PP_FileOpenFlags_Dev flag combination into a corresponding
// PlatformFileFlags flag combination.
// Returns |true| if okay.
bool PepperFileOpenFlagsToPlatformFileFlags(int32_t pp_open_flags,
                                            int* flags_out);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_FILE_TYPE_CONVERSIONS_H_
