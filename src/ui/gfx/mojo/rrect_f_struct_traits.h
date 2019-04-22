// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_RRECT_F_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_RRECT_F_STRUCT_TRAITS_H_

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/mojo/rrect_f.mojom-shared.h"
#include "ui/gfx/rrect_f.h"

namespace mojo {

namespace {

gfx::mojom::RRectFType GfxRRectFTypeToMojo(gfx::RRectF::Type type) {
  switch (type) {
    case gfx::RRectF::Type::kEmpty:
      return gfx::mojom::RRectFType::kEmpty;
    case gfx::RRectF::Type::kRect:
      return gfx::mojom::RRectFType::kRect;
    case gfx::RRectF::Type::kSingle:
      return gfx::mojom::RRectFType::kSingle;
    case gfx::RRectF::Type::kSimple:
      return gfx::mojom::RRectFType::kSimple;
    case gfx::RRectF::Type::kOval:
      return gfx::mojom::RRectFType::kOval;
    case gfx::RRectF::Type::kComplex:
      return gfx::mojom::RRectFType::kComplex;
  }
  NOTREACHED();
  return gfx::mojom::RRectFType::kEmpty;
}

gfx::RRectF::Type MojoRRectFTypeToGfx(gfx::mojom::RRectFType type) {
  switch (type) {
    case gfx::mojom::RRectFType::kEmpty:
      return gfx::RRectF::Type::kEmpty;
    case gfx::mojom::RRectFType::kRect:
      return gfx::RRectF::Type::kRect;
    case gfx::mojom::RRectFType::kSingle:
      return gfx::RRectF::Type::kSingle;
    case gfx::mojom::RRectFType::kSimple:
      return gfx::RRectF::Type::kSimple;
    case gfx::mojom::RRectFType::kOval:
      return gfx::RRectF::Type::kOval;
    case gfx::mojom::RRectFType::kComplex:
      return gfx::RRectF::Type::kComplex;
  }
  NOTREACHED();
  return gfx::RRectF::Type::kEmpty;
}

}  // namespace

template <>
struct StructTraits<gfx::mojom::RRectFDataView, gfx::RRectF> {
  static gfx::mojom::RRectFType type(const gfx::RRectF& input) {
    return GfxRRectFTypeToMojo(input.GetType());
  }

  static gfx::RectF rect(const gfx::RRectF& input) { return input.rect(); }

  static gfx::Vector2dF upper_left(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft);
  }

  static gfx::Vector2dF upper_right(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kUpperRight);
  }

  static gfx::Vector2dF lower_right(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kLowerRight);
  }

  static gfx::Vector2dF lower_left(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft);
  }

  static bool Read(gfx::mojom::RRectFDataView data, gfx::RRectF* out) {
    gfx::RRectF::Type type(MojoRRectFTypeToGfx(data.type()));
    gfx::RectF rect;
    if (!data.ReadRect(&rect))
      return false;
    if (type <= gfx::RRectF::Type::kRect) {
      *out = gfx::RRectF(rect);
      return true;
    }
    gfx::Vector2dF upper_left;
    if (!data.ReadUpperLeft(&upper_left))
      return false;
    *out = gfx::RRectF(rect, upper_left.x(), upper_left.y());
    if (type <= gfx::RRectF::Type::kSimple)
      return true;
    gfx::Vector2dF upper_right;
    gfx::Vector2dF lower_right;
    gfx::Vector2dF lower_left;
    if (!data.ReadUpperRight(&upper_right) ||
        !data.ReadLowerRight(&lower_right) ||
        !data.ReadLowerLeft(&lower_left)) {
      return false;
    }
    out->SetCornerRadii(gfx::RRectF::Corner::kUpperRight, upper_right);
    out->SetCornerRadii(gfx::RRectF::Corner::kLowerRight, lower_right);
    out->SetCornerRadii(gfx::RRectF::Corner::kLowerLeft, lower_left);
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_RRECT_F_STRUCT_TRAITS_H_
