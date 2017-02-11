// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/GraphicsContextState.h"

#include "platform/graphics/skia/SkiaUtils.h"

namespace blink {

static inline SkFilterQuality filterQualityForPaint(
    InterpolationQuality quality) {
  // The filter quality "selected" here will primarily be used when painting a
  // primitive using one of the PaintFlags below. For the most part this will
  // not affect things that are part of the Image class hierarchy (which use
  // the unmodified m_interpolationQuality.)
  return quality != InterpolationNone ? kLow_SkFilterQuality
                                      : kNone_SkFilterQuality;
}

GraphicsContextState::GraphicsContextState()
    : m_textDrawingMode(TextModeFill),
      m_interpolationQuality(InterpolationDefault),
      m_saveCount(0),
      m_shouldAntialias(true) {
  m_strokeFlags.setStyle(PaintFlags::kStroke_Style);
  m_strokeFlags.setStrokeWidth(SkFloatToScalar(m_strokeData.thickness()));
  m_strokeFlags.setStrokeCap(PaintFlags::kDefault_Cap);
  m_strokeFlags.setStrokeJoin(PaintFlags::kDefault_Join);
  m_strokeFlags.setStrokeMiter(SkFloatToScalar(m_strokeData.miterLimit()));
  m_strokeFlags.setFilterQuality(filterQualityForPaint(m_interpolationQuality));
  m_strokeFlags.setAntiAlias(m_shouldAntialias);
  m_fillFlags.setFilterQuality(filterQualityForPaint(m_interpolationQuality));
  m_fillFlags.setAntiAlias(m_shouldAntialias);
}

GraphicsContextState::GraphicsContextState(const GraphicsContextState& other)
    : m_strokeFlags(other.m_strokeFlags),
      m_fillFlags(other.m_fillFlags),
      m_strokeData(other.m_strokeData),
      m_textDrawingMode(other.m_textDrawingMode),
      m_interpolationQuality(other.m_interpolationQuality),
      m_saveCount(0),
      m_shouldAntialias(other.m_shouldAntialias) {}

void GraphicsContextState::copy(const GraphicsContextState& source) {
  this->~GraphicsContextState();
  new (this) GraphicsContextState(source);
}

const PaintFlags& GraphicsContextState::strokeFlags(
    int strokedPathLength) const {
  m_strokeData.setupPaintDashPathEffect(&m_strokeFlags, strokedPathLength);
  return m_strokeFlags;
}

void GraphicsContextState::setStrokeStyle(StrokeStyle style) {
  m_strokeData.setStyle(style);
}

void GraphicsContextState::setStrokeThickness(float thickness) {
  m_strokeData.setThickness(thickness);
  m_strokeFlags.setStrokeWidth(SkFloatToScalar(thickness));
}

void GraphicsContextState::setStrokeColor(const Color& color) {
  m_strokeFlags.setColor(color.rgb());
  m_strokeFlags.setShader(0);
}

void GraphicsContextState::setLineCap(LineCap cap) {
  m_strokeData.setLineCap(cap);
  m_strokeFlags.setStrokeCap((PaintFlags::Cap)cap);
}

void GraphicsContextState::setLineJoin(LineJoin join) {
  m_strokeData.setLineJoin(join);
  m_strokeFlags.setStrokeJoin((PaintFlags::Join)join);
}

void GraphicsContextState::setMiterLimit(float miterLimit) {
  m_strokeData.setMiterLimit(miterLimit);
  m_strokeFlags.setStrokeMiter(SkFloatToScalar(miterLimit));
}

void GraphicsContextState::setFillColor(const Color& color) {
  m_fillFlags.setColor(color.rgb());
  m_fillFlags.setShader(0);
}

// Shadow. (This will need tweaking if we use draw loopers for other things.)
void GraphicsContextState::setDrawLooper(sk_sp<SkDrawLooper> drawLooper) {
  // Grab a new ref for stroke.
  m_strokeFlags.setLooper(drawLooper);
  // Pass the existing ref to fill (to minimize refcount churn).
  m_fillFlags.setLooper(std::move(drawLooper));
}

void GraphicsContextState::setLineDash(const DashArray& dashes,
                                       float dashOffset) {
  m_strokeData.setLineDash(dashes, dashOffset);
}

void GraphicsContextState::setColorFilter(sk_sp<SkColorFilter> colorFilter) {
  // Grab a new ref for stroke.
  m_strokeFlags.setColorFilter(colorFilter);
  // Pass the existing ref to fill (to minimize refcount churn).
  m_fillFlags.setColorFilter(std::move(colorFilter));
}

void GraphicsContextState::setInterpolationQuality(
    InterpolationQuality quality) {
  m_interpolationQuality = quality;
  m_strokeFlags.setFilterQuality(filterQualityForPaint(quality));
  m_fillFlags.setFilterQuality(filterQualityForPaint(quality));
}

void GraphicsContextState::setShouldAntialias(bool shouldAntialias) {
  m_shouldAntialias = shouldAntialias;
  m_strokeFlags.setAntiAlias(shouldAntialias);
  m_fillFlags.setAntiAlias(shouldAntialias);
}

}  // namespace blink
