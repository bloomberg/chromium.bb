// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_COPY_TO_USTRING_H_
#define DEVICE_VR_UTIL_COPY_TO_USTRING_H_

#include "base/strings/string16.h"

namespace device {

// Cross-platform way of copying a string to fixed-length UTF-16 buffer. When
// copying src to dest, the contents will be truncated if necessary.
// TODO(https://crbug.com/957806): Code that does the same thing is copied in
// several places across device/vr. Consolidate them and this function into one
// shared among device/gamepad and device/vr.
void CopyToUString(const base::string16& src,
                   base::char16* dest,
                   size_t dest_length);

}  // namespace device

#endif  // DEVICE_VR_UTIL_COPY_TO_USTRING_H_
