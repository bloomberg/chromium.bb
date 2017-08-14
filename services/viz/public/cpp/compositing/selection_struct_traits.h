// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SELECTION_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SELECTION_STRUCT_TRAITS_H_

#include "cc/input/selection.h"
#include "services/viz/public/interfaces/compositing/selection.mojom-shared.h"
#include "ui/gfx/selection_bound.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SelectionDataView,
                    cc::Selection<gfx::SelectionBound>> {
  static const gfx::SelectionBound& start(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.start;
  }

  static const gfx::SelectionBound& end(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.end;
  }

  static bool Read(viz::mojom::SelectionDataView data,
                   cc::Selection<gfx::SelectionBound>* out) {
    return data.ReadStart(&out->start) && data.ReadEnd(&out->end);
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SELECTION_STRUCT_TRAITS_H_
