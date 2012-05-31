// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include "base/logging.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace drag_utils {

void SetDragImageOnDataObject(const gfx::ImageSkia& image,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              ui::OSExchangeData* data_object) {
  ui::OSExchangeDataProviderAura& provider(
      static_cast<ui::OSExchangeDataProviderAura&>(data_object->provider()));
  provider.set_drag_image(image);
  provider.set_drag_image_offset(cursor_offset);
}

}  // namespace drag_utils
