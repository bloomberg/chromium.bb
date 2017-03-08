// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"

namespace drag_utils {

void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  gfx::ImageSkia image = gfx::ImageSkia(canvas.ExtractImageRep());
  SetDragImageOnDataObject(image, cursor_offset, data_object);
}

#if !defined(OS_WIN)
void SetDragImageOnDataObject(const gfx::ImageSkia& image_skia,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  data_object->provider().SetDragImage(image_skia, cursor_offset);
}
#endif

}  // namespace drag_utils
