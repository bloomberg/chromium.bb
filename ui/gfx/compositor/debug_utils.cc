// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NDEBUG

#include "ui/gfx/compositor/debug_utils.h"

#include <iomanip>
#include <iostream>
#include <string>

#include "base/utf_string_conversions.h"
#include "base/logging.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

void PrintLayerHierarchyImp(const Layer* layer, int indent) {
  if (!layer->visible())
    return;

  std::wostringstream buf;
  std::string indent_str(indent, ' ');
  std::string content_indent_str(indent+1, ' ');

  buf << UTF8ToWide(indent_str);
  buf << L'+' << UTF8ToWide(layer->name()) << L' ' << layer;

  buf << L'\n' << UTF8ToWide(content_indent_str);
  buf << L"bounds: " << layer->bounds().x() << L',' << layer->bounds().y();
  buf << L' ' << layer->bounds().width() << L'x' << layer->bounds().height();

  if (!layer->hole_rect().IsEmpty()) {
    buf << L'\n' << UTF8ToWide(content_indent_str);
    gfx::Rect hole_rect = layer->hole_rect();
    buf << L"hole: " << hole_rect.x() << L',' << hole_rect.y();
    buf << L' ' << hole_rect.width() << L'x' << hole_rect.height();
  }

  if (layer->opacity() != 1.0f) {
    buf << L'\n' << UTF8ToWide(content_indent_str);
    buf << L"opacity: " << std::setprecision(2) << layer->opacity();
  }

  if (layer->transform().HasChange()) {
    gfx::Point translation;
    float rotation;
    gfx::Point3f scale;
    if (ui::InterpolatedTransform::FactorTRS(layer->transform(),
                                             &translation,
                                             &rotation,
                                             &scale)) {
      if (translation != gfx::Point()) {
        buf << L'\n' << UTF8ToWide(content_indent_str);
        buf << L"translation: " << translation.x() << L", " << translation.y();
      }

      if (fabs(rotation) > 1e-5) {
        buf << L'\n' << UTF8ToWide(content_indent_str);
        buf << L"rotation: " << std::setprecision(4) << rotation;
      }

      if (scale.AsPoint() != gfx::Point()) {
        buf << L'\n' << UTF8ToWide(content_indent_str);
        buf << std::setprecision(4);
        buf << L"scale: " << scale.x() << L", " << scale.y();
      }
    }
  }

  VLOG(1) << buf.str();
  std::cout << buf.str() << std::endl;

  for (size_t i = 0, count = layer->children().size(); i < count; ++i)
    PrintLayerHierarchyImp(layer->children()[i], indent + 3);
}

}  // namespace

void PrintLayerHierarchy(const Layer* layer) {
  PrintLayerHierarchyImp(layer, 0);
}

} // namespace ui

#endif // NDEBUG
