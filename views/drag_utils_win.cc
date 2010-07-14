// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/drag_utils.h"

#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "app/os_exchange_data.h"
#include "app/os_exchange_data_provider_win.h"
#include "gfx/canvas_skia.h"
#include "gfx/gdi_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace drag_utils {

static void SetDragImageOnDataObject(HBITMAP hbitmap,
                                     const gfx::Size& size,
                                     const gfx::Point& cursor_offset,
                                     IDataObject* data_object) {
  IDragSourceHelper* helper = NULL;
  HRESULT rv = CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
      IID_IDragSourceHelper, reinterpret_cast<LPVOID*>(&helper));
  if (SUCCEEDED(rv)) {
    SHDRAGIMAGE sdi;
    sdi.sizeDragImage = size.ToSIZE();
    sdi.crColorKey = 0xFFFFFFFF;
    sdi.hbmpDragImage = hbitmap;
    sdi.ptOffset = cursor_offset.ToPOINT();
    helper->InitializeFromBitmap(&sdi, data_object);
  }
};

// Blit the contents of the canvas to a new HBITMAP. It is the caller's
// responsibility to release the |bits| buffer.
static HBITMAP CreateBitmapFromCanvas(const gfx::CanvasSkia& canvas,
                                      int width,
                                      int height) {
  HDC screen_dc = GetDC(NULL);
  BITMAPINFOHEADER header;
  gfx::CreateBitmapHeader(width, height, &header);
  void* bits;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, reinterpret_cast<BITMAPINFO*>(&header),
                       DIB_RGB_COLORS, &bits, NULL, 0);
  HDC compatible_dc = CreateCompatibleDC(screen_dc);
  HGDIOBJ old_object = SelectObject(compatible_dc, bitmap);
  BitBlt(compatible_dc, 0, 0, width, height,
         canvas.getTopPlatformDevice().getBitmapDC(),
         0, 0, SRCCOPY);
  SelectObject(compatible_dc, old_object);
  ReleaseDC(NULL, compatible_dc);
  ReleaseDC(NULL, screen_dc);
  return bitmap;
}

void SetDragImageOnDataObject(const SkBitmap& sk_bitmap,
                              const gfx::Size& size,
                              const gfx::Point& cursor_offset,
                              OSExchangeData* data_object) {
  gfx::CanvasSkia canvas(sk_bitmap.width(), sk_bitmap.height(),
                         /*is_opaque=*/false);
  canvas.DrawBitmapInt(sk_bitmap, 0, 0);

  DCHECK(data_object && !size.IsEmpty());
  // SetDragImageOnDataObject(HBITMAP) takes ownership of the bitmap.
  HBITMAP bitmap = CreateBitmapFromCanvas(canvas, size.width(), size.height());

  // Attach 'bitmap' to the data_object.
  SetDragImageOnDataObject(bitmap, size, cursor_offset,
      OSExchangeDataProviderWin::GetIDataObject(*data_object));
}

} // namespace drag_utils
