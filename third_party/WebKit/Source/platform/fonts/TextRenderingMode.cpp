// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/TextRenderingMode.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String ToString(TextRenderingMode mode) {
  switch (mode) {
    case TextRenderingMode::kAutoTextRendering:
      return "Auto";
    case TextRenderingMode::kOptimizeSpeed:
      return "OptimizeSpeed";
    case TextRenderingMode::kOptimizeLegibility:
      return "OptimizeLegibility";
    case TextRenderingMode::kGeometricPrecision:
      return "GeometricPrecision";
  }
  return "Unknown";
}

}  // namespace blink
