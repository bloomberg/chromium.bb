// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_MOJOM_CLIPBOARD_STRUCT_TRAITS_H_
#define UI_BASE_CLIPBOARD_MOJOM_CLIPBOARD_STRUCT_TRAITS_H_

#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/clipboard/mojom/clipboard.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::ClipboardType, ui::ClipboardType> {
  static ui::mojom::ClipboardType ToMojom(ui::ClipboardType clipboard_type) {
    switch (clipboard_type) {
      case ui::CLIPBOARD_TYPE_COPY_PASTE:
        return ui::mojom::ClipboardType::kCopyPaste;
      case ui::CLIPBOARD_TYPE_SELECTION:
        return ui::mojom::ClipboardType::kSelection;
      case ui::CLIPBOARD_TYPE_DRAG:
        return ui::mojom::ClipboardType::kDrag;
    }
    NOTREACHED();
    return ui::mojom::ClipboardType::kCopyPaste;
  }

  static bool FromMojom(ui::mojom::ClipboardType clipboard_type,
                        ui::ClipboardType* out) {
    switch (clipboard_type) {
      case ui::mojom::ClipboardType::kCopyPaste:
        *out = ui::CLIPBOARD_TYPE_COPY_PASTE;
        return true;
      case ui::mojom::ClipboardType::kSelection:
        *out = ui::CLIPBOARD_TYPE_SELECTION;
        return true;
      case ui::mojom::ClipboardType::kDrag:
        *out = ui::CLIPBOARD_TYPE_DRAG;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // UI_BASE_CLIPBOARD_MOJOM_CLIPBOARD_STRUCT_TRAITS_H_
