// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_

#include "services/viz/public/interfaces/compositing/frame_deadline.mojom.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameDeadlineDataView, uint32_t> {
  static uint32_t value(const uint32_t& input) { return input; }

  static bool Read(viz::mojom::FrameDeadlineDataView data, uint32_t* out) {
    *out = data.value();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_STRUCT_TRAITS_H_
