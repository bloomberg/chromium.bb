// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz_views/surface_element.h"

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace ui_devtools {

SurfaceElement::SurfaceElement(const viz::SurfaceId& surface_id,
                               viz::FrameSinkManagerImpl* frame_sink_manager,
                               UIElementDelegate* ui_element_delegate,
                               UIElement* parent,
                               bool is_detached)
    : UIElement(UIElementType::SURFACE, ui_element_delegate, parent),
      surface_id_(surface_id),
      frame_sink_manager_(frame_sink_manager) {
  viz::Surface* surface =
      frame_sink_manager_->surface_manager()->GetSurfaceForId(surface_id_);
  DCHECK(surface);
  surface_bounds_ = gfx::Rect(surface->size_in_pixels());
}

SurfaceElement::~SurfaceElement() = default;

void SurfaceElement::GetBounds(gfx::Rect* bounds) const {
  // We cannot really know real bounds on the surface unless we do
  // aggregation. Here we just return size of the surface.
  *bounds = surface_bounds_;
}

void SurfaceElement::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void SurfaceElement::GetVisible(bool* visible) const {
  // Currently not real data.
  *visible = true;
}

void SurfaceElement::SetVisible(bool visible) {}

std::unique_ptr<protocol::Array<std::string>> SurfaceElement::GetAttributes()
    const {
  auto attributes = protocol::Array<std::string>::create();
  attributes->addItem("SurfaceId");
  attributes->addItem(surface_id_.ToString());
  attributes->addItem("FrameSink Debug Label");
  attributes->addItem(
      frame_sink_manager_->GetFrameSinkDebugLabel(surface_id_.frame_sink_id())
          .data());
  return attributes;
}

std::pair<gfx::NativeWindow, gfx::Rect> SurfaceElement::GetNodeWindowAndBounds()
    const {
  return {};
}

// static
const viz::SurfaceId& SurfaceElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::SURFACE, element->type());
  return static_cast<const SurfaceElement*>(element)->surface_id_;
}

}  // namespace ui_devtools