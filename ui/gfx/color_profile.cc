// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/color_profile.h"

namespace gfx {

namespace {
static const size_t kMinProfileLength = 128;
static const size_t kMaxProfileLength = 4 * 1024 * 1024;
}

ColorProfile::ColorProfile(const std::vector<char>& profile)
    : profile_(profile) {
  if (!IsValidProfileLength(profile_.size()))
    profile_.clear();
}

ColorProfile::ColorProfile() = default;
ColorProfile::ColorProfile(ColorProfile&& other) = default;
ColorProfile::ColorProfile(const ColorProfile& other) = default;
ColorProfile& ColorProfile::operator=(const ColorProfile& other) = default;
ColorProfile::~ColorProfile() = default;

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
ColorProfile ColorProfile::GetFromBestMonitor() {
  return ColorProfile();
}
#endif

// static
bool ColorProfile::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
