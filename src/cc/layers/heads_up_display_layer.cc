// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include <algorithm>

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return base::WrapRefCounted(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : typeface_(SkTypeface::MakeFromName("times new roman", SkFontStyle())) {
  if (!typeface_) {
    typeface_ = SkTypeface::MakeFromName("monospace", SkFontStyle::Bold());
  }
  DCHECK(typeface_.get());
  SetIsDrawable(true);
  UpdateDrawsContent(HasDrawableContent());
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() = default;

bool HeadsUpDisplayLayer::HasDrawableContent() const {
  return true;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

void HeadsUpDisplayLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "HeadsUpDisplayLayer::PushPropertiesTo");
  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_);
}

}  // namespace cc
