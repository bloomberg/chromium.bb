// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_VMO_UTIL_H_
#define WEBRUNNER_BROWSER_VMO_UTIL_H_

#include <fuchsia/mem/cpp/fidl.h>
#include <lib/zx/channel.h>

#include "base/strings/utf_string_conversions.h"

namespace webrunner {

// Reads the contents of |buffer|, encoded in UTF-8, to a UTF-16 string.
// Returns |false| if |buffer| is not valid UTF-8.
bool ReadUTF8FromVMOAsUTF16(const fuchsia::mem::Buffer& buffer,
                            base::string16* output);

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_VMO_UTIL_H_
