// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace gfx {

class ICCProfile;

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class GFX_EXPORT ColorSpace {
 public:
  ColorSpace();
  static ColorSpace CreateSRGB();

  // TODO: Remove these, and replace with more generic constructors.
  static ColorSpace CreateJpeg();
  static ColorSpace CreateREC601();
  static ColorSpace CreateREC709();

  bool operator==(const ColorSpace& other) const;

 private:
  bool valid_ = false;
  // This is used to look up the ICCProfile from which this ColorSpace was
  // created, if possible.
  uint64_t icc_profile_id_ = 0;

  friend class ICCProfile;
  friend struct IPC::ParamTraits<gfx::ColorSpace>;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
