// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DRAG_UTILS_H_
#define VIEWS_DRAG_UTILS_H_
#pragma once

#include <string>

#include "base/file_path.h"

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
void SetURLAndDragImage(const GURL& url,
                        const std::wstring& title,
                        const SkBitmap& icon,
                        OSExchangeData* data);

// Creates a dragging image to be displayed when the user drags a file from
// Chrome (via the download manager, for example). The drag image is set into
// the supplied data_object. 'file_name' can be a full path, but the directory
// portion will be truncated in the drag image.
void CreateDragImageForFile(const FilePath& file_name,
                            const SkBitmap* icon,
                            OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied canvas. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              OSExchangeData* data_object);
//
// Sets the drag image on data_object from the supplied bitmap. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
void SetDragImageOnDataObject(const SkBitmap& bitmap,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              OSExchangeData* data_object);
} // namespace drag_utils

#endif  // #ifndef VIEWS_DRAG_UTILS_H_
