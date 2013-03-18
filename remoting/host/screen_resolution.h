// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCREEN_RESOLUTION_H_
#define REMOTING_HOST_SCREEN_RESOLUTION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

// Describes a screen's dimensions and DPI.
class ScreenResolution {
 public:
  ScreenResolution();

  ScreenResolution(const SkISize& dimensions, const SkIPoint& dpi);

  // Returns the screen dimensions scaled according to the passed |new_dpi|.
  SkISize ScaleDimensionsToDpi(const SkIPoint& new_dpi) const;

  // Returns true if |dimensions_| specifies an empty rectangle or when
  // IsValid() returns false.
  bool IsEmpty() const;

  // Returns true if both |dimensions_| and |dpi_| are valid. |dimensions_|
  // specifying an empty rectangle is considered to be valid.
  bool IsValid() const;

  // Dimensions of the screen in pixels.
  SkISize dimensions_;

  // The vertical and horizontal DPI of the screen.
  SkIPoint dpi_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCREEN_RESOLUTION_H_
