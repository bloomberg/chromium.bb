// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryStructTraits_h
#define GeometryStructTraits_h

#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/gfx/geometry/mojo/geometry.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::PointFDataView, ::blink::WebFloatPoint> {
  static float x(const ::blink::WebFloatPoint& point) { return point.x; }
  static float y(const ::blink::WebFloatPoint& point) { return point.y; }
  static bool Read(gfx::mojom::PointFDataView, ::blink::WebFloatPoint* out);
};

template <>
struct StructTraits<gfx::mojom::RectFDataView, ::blink::WebFloatRect> {
  static float x(const ::blink::WebFloatRect& rect) { return rect.x; }
  static float y(const ::blink::WebFloatRect& rect) { return rect.y; }
  static float width(const ::blink::WebFloatRect& rect) { return rect.width; }
  static float height(const ::blink::WebFloatRect& rect) { return rect.height; }
  static bool Read(gfx::mojom::RectFDataView, ::blink::WebFloatRect* out);
};

template <>
struct StructTraits<gfx::mojom::RectDataView, ::blink::WebRect> {
  static int x(const ::blink::WebRect& rect) { return rect.x; }
  static int y(const ::blink::WebRect& rect) { return rect.y; }
  static int width(const ::blink::WebRect& rect) { return rect.width; }
  static int height(const ::blink::WebRect& rect) { return rect.height; }
  static bool Read(gfx::mojom::RectDataView, ::blink::WebRect* out);
};

template <>
struct StructTraits<gfx::mojom::SizeDataView, ::blink::WebSize> {
  static int width(const ::blink::WebSize& size) { return size.width; }
  static int height(const ::blink::WebSize& size) { return size.height; }
  static bool Read(gfx::mojom::SizeDataView, ::blink::WebSize* out);
};

}  // namespace mojo

#endif  // GeometryStructTraits_h
