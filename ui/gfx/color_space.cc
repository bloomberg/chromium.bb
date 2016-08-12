// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/icc_profile.h"

namespace gfx {

ColorSpace::ColorSpace()
    : primaries_(PrimaryID::UNSPECIFIED),
      transfer_(TransferID::UNSPECIFIED),
      matrix_(MatrixID::UNSPECIFIED),
      range_(RangeID::LIMITED) {}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range) {
  // TODO: Set profile_id_
}

// static
ColorSpace ColorSpace::CreateSRGB() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                    RangeID::FULL);
}

// Static
ColorSpace ColorSpace::CreateXYZD50() {
  return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateJpeg() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::BT709,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateREC601() {
  return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                    MatrixID::SMPTE170M, RangeID::LIMITED);
}

// static
ColorSpace ColorSpace::CreateREC709() {
  return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                    RangeID::LIMITED);
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  return primaries_ == other.primaries_ && transfer_ == other.transfer_ &&
         matrix_ == other.matrix_ && range_ == other.range_;
}

bool ColorSpace::operator!=(const ColorSpace& other) const {
  return !(*this == other);
}

bool ColorSpace::operator<(const ColorSpace& other) const {
  if (primaries_ < other.primaries_)
    return true;
  if (primaries_ > other.primaries_)
    return false;
  if (transfer_ < other.transfer_)
    return true;
  if (transfer_ > other.transfer_)
    return false;
  if (matrix_ < other.matrix_)
    return true;
  if (matrix_ > other.matrix_)
    return false;
  if (range_ < other.range_)
    return true;
  if (range_ > other.range_)
    return true;

  // TODO(hubbe): For "CUSTOM" primaries or tranfer functions, compare their
  // coefficients here

  return false;
}

}  // namespace gfx
