// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DRAG_UTILS_H_
#define UI_BASE_DRAGDROP_DRAG_UTILS_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "ui/base/ui_export.h"

class GURL;

namespace gfx {
class Canvas;
class ImageSkia;
class Point;
class Size;
}

namespace ui {
class OSExchangeData;
}
using ui::OSExchangeData;

namespace drag_utils {

// Creates a dragging image to be displayed when the user drags a file from
// Chrome (via the download manager, for example). The drag image is set into
// the supplied data_object. 'file_name' can be a full path, but the directory
// portion will be truncated in the drag image.
UI_EXPORT void CreateDragImageForFile(const FilePath& file_name,
                                      const gfx::ImageSkia* icon,
                                      ui::OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied canvas. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
UI_EXPORT void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                                        const gfx::Size& size,
                                        const gfx::Point& cursor_offset,
                                        ui::OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied ImageSkia. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
UI_EXPORT void SetDragImageOnDataObject(const gfx::ImageSkia& image_skia,
                                        const gfx::Size& size,
                                        const gfx::Point& cursor_offset,
                                        ui::OSExchangeData* data_object);

}  // namespace drag_utils

#endif  // UI_BASE_DRAGDROP_DRAG_UTILS_H_
