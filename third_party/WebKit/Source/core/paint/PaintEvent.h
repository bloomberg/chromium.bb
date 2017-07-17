// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintEvent_h
#define PaintEvent_h

namespace blink {

// Paint events that either PaintTiming or FirstMeaningfulPaintDetector receive
// GPU swap times for.
enum class PaintEvent {
  kFirstPaint,
  kFirstContentfulPaint,
  kProvisionalFirstMeaningfulPaint,
};

}  // namespace blink

#endif
