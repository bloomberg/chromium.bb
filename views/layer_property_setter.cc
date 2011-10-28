// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/layer_property_setter.h"

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace views {

namespace {

// DefaultSetter ---------------------------------------------------------------

class DefaultSetter : public LayerPropertySetter {
 public:
  DefaultSetter();

  // LayerPropertySetter:
  virtual void Installed(ui::Layer* layer) OVERRIDE;
  virtual void Uninstalled(ui::Layer* layer) OVERRIDE;
  virtual void SetTransform(ui::Layer* layer,
                            const ui::Transform& transform) OVERRIDE;
  virtual void SetBounds(ui::Layer* layer, const gfx::Rect& bounds) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultSetter);
};

DefaultSetter::DefaultSetter() {
}

void DefaultSetter::Installed(ui::Layer* layer) {
}

void DefaultSetter::Uninstalled(ui::Layer* layer) {
}

void DefaultSetter::SetTransform(ui::Layer* layer,
                                 const ui::Transform& transform) {
  layer->SetTransform(transform);
}

void DefaultSetter::SetBounds(ui::Layer* layer, const gfx::Rect& bounds) {
  layer->SetBounds(bounds);
}

}  // namespace

// LayerPropertySetter ---------------------------------------------------------

// static
LayerPropertySetter* LayerPropertySetter::CreateDefaultSetter() {
  return new DefaultSetter;
}

}  // namespace views
