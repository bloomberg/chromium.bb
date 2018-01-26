// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_

#include "services/viz/public/interfaces/compositing/frame_deadline.mojom.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameDeadlineDataView, viz::FrameDeadline> {
  static uint32_t value(const viz::FrameDeadline& input) {
    return input.value();
  }

  static bool use_default_lower_bound_deadline(
      const viz::FrameDeadline& input) {
    return input.use_default_lower_bound_deadline();
  }

  static bool Read(viz::mojom::FrameDeadlineDataView data,
                   viz::FrameDeadline* out) {
    *out = viz::FrameDeadline(data.value(),
                              data.use_default_lower_bound_deadline());
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_
