// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineVerticalPositionType_h
#define LineVerticalPositionType_h

namespace blink {

enum class LineVerticalPositionType {
  // TextTop and TextBottom are the top/bottom of the content area.
  // This is where 'vertical-align: text-top/text-bottom' aligns to.
  // This is explicitly undefined in CSS2.
  // https://drafts.csswg.org/css2/visudet.html#inline-non-replaced
  TextTop,
  TextBottom,
  // Em height as being discussed in Font Metrics API.
  // https://drafts.css-houdini.org/font-metrics-api-1/#fontmetrics
  TopOfEmHeight,
  BottomOfEmHeight
};

}  // namespace blink

#endif  // LineVerticalPositionType_h
