// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextPaintStyle_h
#define TextPaintStyle_h

#include "core/CoreExport.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ShadowList;

struct CORE_EXPORT TextPaintStyle {
  STACK_ALLOCATED();
  Color current_color;
  Color fill_color;
  Color stroke_color;
  Color emphasis_mark_color;
  float stroke_width;
  const ShadowList* shadow;

  bool operator==(const TextPaintStyle& other) {
    return current_color == other.current_color &&
           fill_color == other.fill_color &&
           stroke_color == other.stroke_color &&
           emphasis_mark_color == other.emphasis_mark_color &&
           stroke_width == other.stroke_width && shadow == other.shadow;
  }
  bool operator!=(const TextPaintStyle& other) { return !(*this == other); }
};

}  // namespace blink

#endif  // TextPaintStyle_h
