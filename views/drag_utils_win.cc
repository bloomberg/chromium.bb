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
#include "third_party/skia/include/core/SkUnPreMultiply.h"

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
                         /*is_opaque=*/true);
  SkBitmap opaque_bitmap;
  if (sk_bitmap.isOpaque()) {
    opaque_bitmap = sk_bitmap;
  } else {
    // InitializeFromBitmap() doesn't expect an alpha channel and is confused
    // by premultiplied colors, so unpremultiply the bitmap.
    SkBitmap tmp_bitmap;
    tmp_bitmap.setConfig(
        sk_bitmap.config(), sk_bitmap.width(), sk_bitmap.height());
    tmp_bitmap.allocPixels();

    SkAutoLockPixels lock(tmp_bitmap);
    const int kBytesPerPixel = 4;
    for (int y = 0; y < tmp_bitmap.height(); y++) {
      for (int x = 0; x < tmp_bitmap.width(); x++) {
        uint32 src_pixel = *sk_bitmap.getAddr32(x, y);
        uint32* dst_pixel = tmp_bitmap.getAddr32(x, y);
        SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(src_pixel);
        *dst_pixel = unmultiplied;
      }
    }
    tmp_bitmap.setIsOpaque(true);
    opaque_bitmap = tmp_bitmap;
  }
  canvas.DrawBitmapInt(opaque_bitmap, 0, 0);

  DCHECK(data_object && !size.IsEmpty());
  // SetDragImageOnDataObject(HBITMAP) takes ownership of the bitmap.
  HBITMAP bitmap = CreateBitmapFromCanvas(canvas, size.width(), size.height());

  // Attach 'bitmap' to the data_object.
  SetDragImageOnDataObject(bitmap, size, cursor_offset,
      OSExchangeDataProviderWin::GetIDataObject(*data_object));
}

} // namespace drag_utils
