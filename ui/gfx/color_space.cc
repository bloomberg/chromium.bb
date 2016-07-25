// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/icc_profile.h"

namespace gfx {

ColorSpace::ColorSpace() = default;

// static
ColorSpace ColorSpace::CreateSRGB() {
  ColorSpace color_space;
  color_space.valid_ = true;
  color_space.icc_profile_id_ = ICCProfile::kSRGBId;
  return color_space;
}

// static
ColorSpace ColorSpace::CreateJpeg() {
  ColorSpace color_space;
  color_space.valid_ = true;
  color_space.icc_profile_id_ = ICCProfile::kJpegId;
  return color_space;
}

// static
ColorSpace ColorSpace::CreateREC601() {
  ColorSpace color_space;
  color_space.valid_ = true;
  color_space.icc_profile_id_ = ICCProfile::kRec601Id;
  return color_space;
}

// static
ColorSpace ColorSpace::CreateREC709() {
  ColorSpace color_space;
  color_space.valid_ = true;
  color_space.icc_profile_id_ = ICCProfile::kRec709Id;
  return color_space;
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  // TODO(ccameron): The |icc_profile_id_| should eventually not factor in
  // to equality comparison at all, because it's just an optimization, but
  // for now there is no other data in this structure.
  return valid_ == other.valid_ && icc_profile_id_ == other.icc_profile_id_;
}

}  // namespace gfx
