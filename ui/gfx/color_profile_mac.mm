// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace gfx {

// static
ColorProfile ColorProfile::GetFromBestMonitor() {
  ColorProfile profile;
  CGColorSpaceRef monitor_color_space(base::mac::GetSystemColorSpace());
  base::ScopedCFTypeRef<CFDataRef> icc_profile(
      CGColorSpaceCopyICCProfile(monitor_color_space));
  if (!icc_profile)
    return profile;
  size_t length = CFDataGetLength(icc_profile);
  if (!IsValidProfileLength(length))
    return profile;
  const unsigned char* data = CFDataGetBytePtr(icc_profile);
  profile.profile_.assign(data, data + length);
  return profile;
}

}  // namespace gfx
