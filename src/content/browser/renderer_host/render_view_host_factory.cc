// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_factory.h"

#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// static
RenderViewHostFactory* RenderViewHostFactory::factory_ = nullptr;

// static
bool RenderViewHostFactory::is_real_render_view_host_ = false;

// static
RenderViewHost* RenderViewHostFactory::Create(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int32_t main_frame_routing_id,
    bool swapped_out) {
  int32_t routing_id = instance->GetProcess()->GetNextRoutingID();
  int32_t widget_routing_id = instance->GetProcess()->GetNextRoutingID();
  if (factory_) {
    return factory_->CreateRenderViewHost(instance, delegate, widget_delegate,
                                          routing_id, main_frame_routing_id,
                                          widget_routing_id, swapped_out);
  }

  RenderViewHostImpl* view_host = new RenderViewHostImpl(
      instance,
      RenderWidgetHostFactory::Create(widget_delegate, instance->GetProcess(),
                                      widget_routing_id, mojo::NullRemote(),
                                      /*hidden=*/true),
      delegate, routing_id, main_frame_routing_id, swapped_out,
      true /* has_initialized_audio_host */);
  return view_host;
}

// static
void RenderViewHostFactory::RegisterFactory(RenderViewHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderViewHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = nullptr;
}

}  // namespace content
