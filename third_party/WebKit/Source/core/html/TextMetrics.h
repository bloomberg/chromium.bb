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

  float width() const { return width_; }
  void SetWidth(float w) { width_ = w; }

  float actualBoundingBoxLeft() const { return actual_bounding_box_left_; }
  void SetActualBoundingBoxLeft(float actual_bounding_box_left) {
    actual_bounding_box_left_ = actual_bounding_box_left;
  }

  float actualBoundingBoxRight() const { return actual_bounding_box_right_; }
  void SetActualBoundingBoxRight(float actual_bounding_box_right) {
    actual_bounding_box_right_ = actual_bounding_box_right;
  }

  float fontBoundingBoxAscent() const { return font_bounding_box_ascent_; }
  void SetFontBoundingBoxAscent(float font_bounding_box_ascent) {
    font_bounding_box_ascent_ = font_bounding_box_ascent;
  }

  float fontBoundingBoxDescent() const { return font_bounding_box_descent_; }
  void SetFontBoundingBoxDescent(float font_bounding_box_descent) {
    font_bounding_box_descent_ = font_bounding_box_descent;
  }

  float actualBoundingBoxAscent() const { return actual_bounding_box_ascent_; }
  void SetActualBoundingBoxAscent(float actual_bounding_box_ascent) {
    actual_bounding_box_ascent_ = actual_bounding_box_ascent;
  }

  float actualBoundingBoxDescent() const {
    return actual_bounding_box_descent_;
  }
  void SetActualBoundingBoxDescent(float actual_bounding_box_descent) {
    actual_bounding_box_descent_ = actual_bounding_box_descent;
  }

  float emHeightAscent() const { return em_height_ascent_; }
  void SetEmHeightAscent(float em_height_ascent) {
    em_height_ascent_ = em_height_ascent;
  }

  float emHeightDescent() const { return em_height_descent_; }
  void SetEmHeightDescent(float em_height_descent) {
    em_height_descent_ = em_height_descent;
  }

  float hangingBaseline() const { return hanging_baseline_; }
  void SetHangingBaseline(float hanging_baseline) {
    hanging_baseline_ = hanging_baseline;
  }

  float alphabeticBaseline() const { return alphabetic_baseline_; }
  void SetAlphabeticBaseline(float alphabetic_baseline) {
    alphabetic_baseline_ = alphabetic_baseline;
  }

  float ideographicBaseline() const { return ideographic_baseline_; }
  void SetIdeographicBaseline(float ideographic_baseline) {
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
  float width_;
  float actual_bounding_box_left_;
  float actual_bounding_box_right_;

  // y-direction
  float font_bounding_box_ascent_;
  float font_bounding_box_descent_;
  float actual_bounding_box_ascent_;
  float actual_bounding_box_descent_;
  float em_height_ascent_;
  float em_height_descent_;
  float hanging_baseline_;
  float alphabetic_baseline_;
  float ideographic_baseline_;
};

}  // namespace blink

#endif  // TextMetrics_h
