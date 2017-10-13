/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextMetrics_h
#define TextMetrics_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT TextMetrics final : public GarbageCollected<TextMetrics>,
                                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextMetrics* Create() { return new TextMetrics; }

  double width() const { return width_; }
  void SetWidth(double w) { width_ = w; }

  double actualBoundingBoxLeft() const { return actual_bounding_box_left_; }
  void SetActualBoundingBoxLeft(double actual_bounding_box_left) {
    actual_bounding_box_left_ = actual_bounding_box_left;
  }

  double actualBoundingBoxRight() const { return actual_bounding_box_right_; }
  void SetActualBoundingBoxRight(double actual_bounding_box_right) {
    actual_bounding_box_right_ = actual_bounding_box_right;
  }

  double fontBoundingBoxAscent() const { return font_bounding_box_ascent_; }
  void SetFontBoundingBoxAscent(double font_bounding_box_ascent) {
    font_bounding_box_ascent_ = font_bounding_box_ascent;
  }

  double fontBoundingBoxDescent() const { return font_bounding_box_descent_; }
  void SetFontBoundingBoxDescent(double font_bounding_box_descent) {
    font_bounding_box_descent_ = font_bounding_box_descent;
  }

  double actualBoundingBoxAscent() const { return actual_bounding_box_ascent_; }
  void SetActualBoundingBoxAscent(double actual_bounding_box_ascent) {
    actual_bounding_box_ascent_ = actual_bounding_box_ascent;
  }

  double actualBoundingBoxDescent() const {
    return actual_bounding_box_descent_;
  }
  void SetActualBoundingBoxDescent(double actual_bounding_box_descent) {
    actual_bounding_box_descent_ = actual_bounding_box_descent;
  }

  double emHeightAscent() const { return em_height_ascent_; }
  void SetEmHeightAscent(double em_height_ascent) {
    em_height_ascent_ = em_height_ascent;
  }

  double emHeightDescent() const { return em_height_descent_; }
  void SetEmHeightDescent(double em_height_descent) {
    em_height_descent_ = em_height_descent;
  }

  double hangingBaseline() const { return hanging_baseline_; }
  void SetHangingBaseline(double hanging_baseline) {
    hanging_baseline_ = hanging_baseline;
  }

  double alphabeticBaseline() const { return alphabetic_baseline_; }
  void SetAlphabeticBaseline(double alphabetic_baseline) {
    alphabetic_baseline_ = alphabetic_baseline;
  }

  double ideographicBaseline() const { return ideographic_baseline_; }
  void SetIdeographicBaseline(double ideographic_baseline) {
    ideographic_baseline_ = ideographic_baseline;
  }

  DEFINE_INLINE_TRACE() {}

 private:
  TextMetrics()
      : width_(0),
        actual_bounding_box_left_(0),
        actual_bounding_box_right_(0),
        font_bounding_box_ascent_(0),
        font_bounding_box_descent_(0),
        actual_bounding_box_ascent_(0),
        actual_bounding_box_descent_(0),
        em_height_ascent_(0),
        em_height_descent_(0),
        hanging_baseline_(0),
        alphabetic_baseline_(0),
        ideographic_baseline_(0) {}

  // x-direction
  double width_;
  double actual_bounding_box_left_;
  double actual_bounding_box_right_;

  // y-direction
  double font_bounding_box_ascent_;
  double font_bounding_box_descent_;
  double actual_bounding_box_ascent_;
  double actual_bounding_box_descent_;
  double em_height_ascent_;
  double em_height_descent_;
  double hanging_baseline_;
  double alphabetic_baseline_;
  double ideographic_baseline_;
};

}  // namespace blink

#endif  // TextMetrics_h
