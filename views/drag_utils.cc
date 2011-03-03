// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/drag_utils.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "views/controls/button/text_button.h"

using ui::OSExchangeData;

namespace drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageMaxWidth = 200;
static const int kLinkDragImageVPadding = 3;

// File dragging pixel measurements
static const int kFileDragImageMaxWidth = 200;
static const SkColor kFileDragImageTextColor = SK_ColorBLACK;

void SetURLAndDragImage(const GURL& url,
                        const std::wstring& title,
                        const SkBitmap& icon,
                        OSExchangeData* data) {
  DCHECK(url.is_valid() && data);

  data->SetURL(url, WideToUTF16(title));

  // Create a button to render the drag image for us.
  views::TextButton button(NULL,
                           title.empty() ? UTF8ToWide(url.spec()) : title);
  button.set_max_width(kLinkDragImageMaxWidth);
  if (icon.isNull()) {
    button.SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
                   IDR_DEFAULT_FAVICON));
  } else {
    button.SetIcon(icon);
  }
  gfx::Size prefsize = button.GetPreferredSize();
  button.SetBounds(0, 0, prefsize.width(), prefsize.height());

  // Render the image.
  gfx::CanvasSkia canvas(prefsize.width(), prefsize.height(), false);
  button.Paint(&canvas, true);
  SetDragImageOnDataObject(canvas, prefsize,
      gfx::Point(prefsize.width() / 2, prefsize.height() / 2), data);
}

void CreateDragImageForFile(const FilePath& file_name,
                            const SkBitmap* icon,
                            OSExchangeData* data_object) {
  DCHECK(icon);
  DCHECK(data_object);

  // Set up our text portion
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ResourceBundle::BaseFont);

  const int width = kFileDragImageMaxWidth;
  // Add +2 here to allow room for the halo.
  const int height = font.GetHeight() + icon->height() +
                     kLinkDragImageVPadding + 2;
  gfx::CanvasSkia canvas(width, height, false /* translucent */);

  // Paint the icon.
  canvas.DrawBitmapInt(*icon, (width - icon->width()) / 2, 0);

  std::wstring name = UTF16ToWide(file_name.BaseName().LossyDisplayName());
#if defined(OS_WIN)
  // Paint the file name. We inset it one pixel to allow room for the halo.
  canvas.DrawStringWithHalo(name, font, kFileDragImageTextColor, SK_ColorWHITE,
                            1, icon->height() + kLinkDragImageVPadding + 1,
                            width - 2, font.GetHeight(),
                            gfx::Canvas::TEXT_ALIGN_CENTER);
#else
  canvas.DrawStringInt(WideToUTF16Hack(name), font, kFileDragImageTextColor,
                       0, icon->height() + kLinkDragImageVPadding,
                       width, font.GetHeight(), gfx::Canvas::TEXT_ALIGN_CENTER);
#endif

  SetDragImageOnDataObject(canvas, gfx::Size(width, height),
                           gfx::Point(width / 2, kLinkDragImageVPadding),
                           data_object);
}

void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              OSExchangeData* data_object) {
  SetDragImageOnDataObject(
      canvas.AsCanvasSkia()->ExtractBitmap(), size, cursor_offset, data_object);
}

} // namespace drag_utils
