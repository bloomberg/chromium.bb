// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFont_h
#define WebFont_h

#include "WebCanvas.h"
#include "WebColor.h"
#include "WebCommon.h"
#include <memory>

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef DrawText

namespace blink {

struct WebFloatPoint;
struct WebFloatRect;
struct WebFontDescription;
struct WebRect;
struct WebTextRun;

class WebFont {
 public:
  BLINK_PLATFORM_EXPORT static WebFont* Create(const WebFontDescription&);
  BLINK_PLATFORM_EXPORT ~WebFont();

  BLINK_PLATFORM_EXPORT WebFontDescription GetFontDescription() const;
  BLINK_PLATFORM_EXPORT int Ascent() const;
  BLINK_PLATFORM_EXPORT int Descent() const;
  BLINK_PLATFORM_EXPORT int Height() const;
  BLINK_PLATFORM_EXPORT int LineSpacing() const;
  BLINK_PLATFORM_EXPORT float XHeight() const;
  BLINK_PLATFORM_EXPORT void DrawText(WebCanvas*,
                                      const WebTextRun&,
                                      const WebFloatPoint& left_baseline,
                                      WebColor,
                                      const WebRect& clip) const;
  BLINK_PLATFORM_EXPORT int CalculateWidth(const WebTextRun&) const;
  BLINK_PLATFORM_EXPORT int OffsetForPosition(const WebTextRun&,
                                              float position) const;
  BLINK_PLATFORM_EXPORT WebFloatRect
  SelectionRectForText(const WebTextRun&,
                       const WebFloatPoint& left_baseline,
                       int height,
                       int from = 0,
                       int to = -1) const;

 private:
  explicit WebFont(const WebFontDescription&);

  class Impl;
  std::unique_ptr<Impl> private_;
};

}  // namespace blink

#endif
