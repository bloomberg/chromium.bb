// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_SEQUENCE_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_SEQUENCE_STRUCT_TRAITS_H_

#include "components/viz/common/surfaces/surface_sequence.h"
#include "services/viz/public/interfaces/compositing/surface_sequence.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SurfaceSequenceDataView, viz::SurfaceSequence> {
  static const viz::FrameSinkId& frame_sink_id(const viz::SurfaceSequence& id) {
    return id.frame_sink_id;
  }

  static uint32_t sequence(const viz::SurfaceSequence& id) {
    return id.sequence;
  }

  static bool Read(viz::mojom::SurfaceSequenceDataView data,
                   viz::SurfaceSequence* out) {
    viz::FrameSinkId frame_sink_id;
    if (!data.ReadFrameSinkId(&frame_sink_id))
      return false;
    *out = viz::SurfaceSequence(frame_sink_id, data.sequence());
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_SEQUENCE_STRUCT_TRAITS_H_
