// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageVPadding = 3;

// File dragging pixel measurements
static const int kFileDragImageMaxWidth = 200;
static const SkColor kFileDragImageTextColor = SK_ColorBLACK;

void CreateDragImageForFile(const FilePath& file_name,
                            const gfx::ImageSkia* icon,
                            ui::OSExchangeData* data_object) {
  DCHECK(icon);
  DCHECK(data_object);

  // Set up our text portion
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ResourceBundle::BaseFont);

  const int width = kFileDragImageMaxWidth;
  // Add +2 here to allow room for the halo.
  const int height = font.GetHeight() + icon->height() +
                     kLinkDragImageVPadding + 2;
  gfx::Canvas canvas(gfx::Size(width, height), false /* translucent */);

  // Paint the icon.
  canvas.DrawImageInt(*icon, (width - icon->width()) / 2, 0);

  string16 name = file_name.BaseName().LossyDisplayName();
  const int flags = gfx::Canvas::TEXT_ALIGN_CENTER;
#if defined(OS_WIN)
  // Paint the file name. We inset it one pixel to allow room for the halo.
  canvas.DrawStringWithHalo(name, font, kFileDragImageTextColor, SK_ColorWHITE,
                            1, icon->height() + kLinkDragImageVPadding + 1,
                            width - 2, font.GetHeight(), flags);
#else
  // NO_SUBPIXEL_RENDERING is required when drawing to a non-opaque canvas.
  canvas.DrawStringInt(name, font, kFileDragImageTextColor,
                       0, icon->height() + kLinkDragImageVPadding,
                       width, font.GetHeight(),
                       flags | gfx::Canvas::NO_SUBPIXEL_RENDERING);
#endif

  SetDragImageOnDataObject(canvas, gfx::Size(width, height),
                           gfx::Point(width / 2, kLinkDragImageVPadding),
                           data_object);
}

void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              ui::OSExchangeData* data_object) {
  SetDragImageOnDataObject(canvas.ExtractBitmap(), size, cursor_offset,
                           data_object);
}

}  // namespace drag_utils
