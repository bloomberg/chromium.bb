// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_
#define UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace mojo {

template <>
struct StructTraits<mojo::Insets, gfx::Insets> {
  static int top(const gfx::Insets& p) { return p.top(); }
  static int left(const gfx::Insets& p) { return p.left(); }
  static int bottom(const gfx::Insets& p) { return p.bottom(); }
  static int right(const gfx::Insets& p) { return p.right(); }
  static bool Read(mojo::InsetsDataView data, gfx::Insets* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<mojo::InsetsF, gfx::InsetsF> {
  static float top(const gfx::InsetsF& p) { return p.top(); }
  static float left(const gfx::InsetsF& p) { return p.left(); }
  static float bottom(const gfx::InsetsF& p) { return p.bottom(); }
  static float right(const gfx::InsetsF& p) { return p.right(); }
  static bool Read(mojo::InsetsFDataView data, gfx::InsetsF* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<mojo::Point, gfx::Point> {
  static int x(const gfx::Point& p) { return p.x(); }
  static int y(const gfx::Point& p) { return p.y(); }
  static bool Read(mojo::PointDataView data, gfx::Point* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<mojo::PointF, gfx::PointF> {
  static float x(const gfx::PointF& p) { return p.x(); }
  static float y(const gfx::PointF& p) { return p.y(); }
  static bool Read(mojo::PointFDataView data, gfx::PointF* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<mojo::Rect, gfx::Rect> {
  static int x(const gfx::Rect& p) { return p.x(); }
  static int y(const gfx::Rect& p) { return p.y(); }
  static int width(const gfx::Rect& p) { return p.width(); }
  static int height(const gfx::Rect& p) { return p.height(); }
  static bool Read(mojo::RectDataView data, gfx::Rect* out) {
    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<mojo::RectF, gfx::RectF> {
  static float x(const gfx::RectF& p) { return p.x(); }
  static float y(const gfx::RectF& p) { return p.y(); }
  static float width(const gfx::RectF& p) { return p.width(); }
  static float height(const gfx::RectF& p) { return p.height(); }
  static bool Read(mojo::RectFDataView data, gfx::RectF* out) {
    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<mojo::Size, gfx::Size> {
  static int width(const gfx::Size& p) { return p.width(); }
  static int height(const gfx::Size& p) { return p.height(); }
  static bool Read(mojo::SizeDataView data, gfx::Size* out) {
    out->SetSize(data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<mojo::SizeF, gfx::SizeF> {
  static float width(const gfx::SizeF& p) { return p.width(); }
  static float height(const gfx::SizeF& p) { return p.height(); }
  static bool Read(mojo::SizeFDataView data, gfx::SizeF* out) {
    out->SetSize(data.width(), data.height());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_
