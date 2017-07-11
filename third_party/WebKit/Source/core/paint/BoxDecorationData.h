// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxDecorationData_h
#define BoxDecorationData_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "platform/graphics/Color.h"

namespace blink {

class LayoutBox;
class Document;
class ComputedStyle;

// Information extracted from ComputedStyle for box painting.
struct BoxDecorationData {
  STACK_ALLOCATED();

 public:
  BoxDecorationData(const LayoutBox&);

  Color background_color;
  BackgroundBleedAvoidance bleed_avoidance;
  bool has_background;
  bool has_border_decoration;
  bool has_appearance;

 private:
  BackgroundBleedAvoidance DetermineBackgroundBleedAvoidance(
      const Document&,
      const ComputedStyle&,
      bool background_should_always_be_clipped);
};

}  // namespace blink

#endif
