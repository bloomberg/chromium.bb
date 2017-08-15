// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_struct_traits.h"

#include "base/trace_event/trace_event.h"
#include "services/viz/public/cpp/compositing/compositor_frame_metadata_struct_traits.h"
#include "services/viz/public/cpp/compositing/render_pass_struct_traits.h"
#include "services/viz/public/cpp/compositing/transferable_resource_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::CompositorFrameDataView, cc::CompositorFrame>::
    Read(viz::mojom::CompositorFrameDataView data, cc::CompositorFrame* out) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "StructTraits::CompositorFrame::Read");
  return data.ReadPasses(&out->render_pass_list) &&
         !out->render_pass_list.empty() && data.ReadMetadata(&out->metadata) &&
         data.ReadResources(&out->resource_list);
}

}  // namespace mojo
