// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DRAG_UTILS_H_
#define UI_VIEWS_DRAG_UTILS_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/string16.h"
#include "ui/views/views_export.h"

class GURL;
class SkBitmap;

namespace gfx {
class Canvas;
class Point;
class Size;
}

namespace ui {
class OSExchangeData;
}
using ui::OSExchangeData;

namespace drag_utils {

// Sets url and title on data as well as setting a suitable image for dragging.
// The image looks like that of the bookmark buttons.
VIEWS_EXPORT void SetURLAndDragImage(const GURL& url,
                                     const string16& title,
                                     const SkBitmap& icon,
                                     ui::OSExchangeData* data);

// Creates a dragging image to be displayed when the user drags a file from
// Chrome (via the download manager, for example). The drag image is set into
// the supplied data_object. 'file_name' can be a full path, but the directory
// portion will be truncated in the drag image.
VIEWS_EXPORT void CreateDragImageForFile(const FilePath& file_name,
                                         const SkBitmap* icon,
                                         ui::OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied canvas. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
VIEWS_EXPORT void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                                           const gfx::Size& size,
                                           const gfx::Point& cursor_offset,
                                           ui::OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied bitmap. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
VIEWS_EXPORT void SetDragImageOnDataObject(const SkBitmap& bitmap,
                                           const gfx::Size& size,
                                           const gfx::Point& cursor_offset,
                                           ui::OSExchangeData* data_object);

}  // namespace drag_utils

#endif  // UI_VIEWS_DRAG_UTILS_H_
