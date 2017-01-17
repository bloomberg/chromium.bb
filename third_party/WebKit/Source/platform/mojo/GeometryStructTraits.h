// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryStructTraits_h
#define GeometryStructTraits_h

#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/gfx/geometry/mojo/geometry.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::SizeDataView, ::blink::WebSize> {
  static int width(const ::blink::WebSize& size) { return size.width; }
  static int height(const ::blink::WebSize& size) { return size.height; }
  static bool Read(gfx::mojom::SizeDataView, ::blink::WebSize* out);
};

}  // namespace mojo

#endif  // GeometryStructTraits_h
