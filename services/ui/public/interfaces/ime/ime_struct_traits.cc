// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/interfaces/ime/ime_struct_traits.h"

#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace mojo {

// static
bool StructTraits<ui::mojom::CompositionUnderlineDataView,
                  ui::CompositionUnderline>::
    Read(ui::mojom::CompositionUnderlineDataView data,
         ui::CompositionUnderline* out) {
  if (data.is_null())
    return false;
  out->start_offset = data.start_offset();
  out->end_offset = data.end_offset();
  out->color = data.color();
  out->thick = data.thick();
  out->background_color = data.background_color();
  return true;
}

// static
bool StructTraits<ui::mojom::CompositionTextDataView, ui::CompositionText>::
    Read(ui::mojom::CompositionTextDataView data, ui::CompositionText* out) {
  return !data.is_null() && data.ReadText(&out->text) &&
         data.ReadUnderlines(&out->underlines) &&
         data.ReadSelection(&out->selection);
}

}  // namespace mojo
