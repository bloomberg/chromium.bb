// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/mojom/ime/ime_struct_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ws::mojom::CandidateWindowPropertiesDataView,
                  ui::CandidateWindow::CandidateWindowProperty>::
    Read(ws::mojom::CandidateWindowPropertiesDataView data,
         ui::CandidateWindow::CandidateWindowProperty* out) {
  if (data.is_null())
    return false;
  if (!data.ReadAuxiliaryText(&out->auxiliary_text))
    return false;
  out->page_size = data.page_size();
  out->is_vertical = data.vertical();
  out->is_auxiliary_text_visible = data.auxiliary_text_visible();
  out->cursor_position = data.cursor_position();
  out->is_cursor_visible = data.cursor_visible();
  out->show_window_at_composition =
      data.window_position() ==
      ws::mojom::CandidateWindowPosition::kComposition;
  return true;
}

// static
bool StructTraits<ws::mojom::CandidateWindowEntryDataView,
                  ui::CandidateWindow::Entry>::
    Read(ws::mojom::CandidateWindowEntryDataView data,
         ui::CandidateWindow::Entry* out) {
  return !data.is_null() && data.ReadValue(&out->value) &&
         data.ReadLabel(&out->label) && data.ReadAnnotation(&out->annotation) &&
         data.ReadDescriptionTitle(&out->description_title) &&
         data.ReadDescriptionBody(&out->description_body);
}

}  // namespace mojo
