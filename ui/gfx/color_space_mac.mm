// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace gfx {

// static
ColorSpace ColorSpace::FromBestMonitor() {
  return FromCGColorSpace(base::mac::GetSystemColorSpace());
}

// static
ColorSpace ColorSpace::FromCGColorSpace(CGColorSpaceRef cg_color_space) {
  base::ScopedCFTypeRef<CFDataRef> cf_icc_profile(
      CGColorSpaceCopyICCProfile(cg_color_space));
  if (!cf_icc_profile)
    return gfx::ColorSpace();
  size_t length = CFDataGetLength(cf_icc_profile);
  const unsigned char* data = CFDataGetBytePtr(cf_icc_profile);
  std::vector<char> vector_icc_profile;
  vector_icc_profile.assign(data, data + length);
  return FromICCProfile(vector_icc_profile);
}

}  // namespace gfx
