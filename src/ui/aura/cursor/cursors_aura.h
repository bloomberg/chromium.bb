// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CURSOR_CURSORS_AURA_H_
#define UI_AURA_CURSOR_CURSORS_AURA_H_

#include "base/component_export.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
enum class CursorSize;
}  // namespace ui

namespace aura {

// Returns data about |id|, where id is a cursor constant like
// ui::mojom::CursorType::kHelp. The IDR will be placed in |resource_id| and
// the hotspots for the different DPIs will be placed in |hot_1x| and
// |hot_2x|.  Returns false if |id| is invalid.
COMPONENT_EXPORT(UI_AURA_CURSOR)
bool GetCursorDataFor(ui::CursorSize cursor_size,
                      ui::mojom::CursorType id,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point);

SkBitmap GetDefaultBitmap(const ui::Cursor& cursor);

gfx::Point GetDefaultHotspot(const ui::Cursor& cursor);

}  // namespace aura

#endif  // UI_AURA_CURSOR_CURSORS_AURA_H_
