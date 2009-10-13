// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "app/win_util.h"
#include "app/gfx/font.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "printing/page_number.h"
#include "printing/page_overlays.h"
#include "printing/printed_pages_source.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

#if defined(OS_WIN)
namespace {

void SimpleModifyWorldTransform(HDC context,
                                int offset_x,
                                int offset_y,
                                double shrink_factor) {
  XFORM xform = { 0 };
  xform.eDx = static_cast<float>(offset_x);
  xform.eDy = static_cast<float>(offset_y);
  xform.eM11 = xform.eM22 = static_cast<float>(1. / shrink_factor);
  BOOL res = ModifyWorldTransform(context, &xform, MWT_LEFTMULTIPLY);
  DCHECK_NE(res, 0);
}

void DrawRect(HDC context, gfx::Rect rect) {
  Rectangle(context, rect.x(), rect.y(), rect.right(), rect.bottom());
}

}  // namespace
#endif  // OS_WIN

namespace printing {

void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page, gfx::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  const printing::PageSetup& page_setup(
      immutable_.settings_.page_setup_pixels());

  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(context);
  DCHECK_NE(saved_state, 0);
  skia::PlatformDevice::InitializeDC(context);
  {
    // Save the state (again) to apply the necessary world transformation.
    int saved_state = SaveDC(context);
    DCHECK_NE(saved_state, 0);

#if 0
    // Debug code to visually verify margins (leaks GDI handles).
    XFORM debug_xform = { 0 };
    ModifyWorldTransform(context, &debug_xform, MWT_IDENTITY);
    // Printable area:
    SelectObject(context, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));
    SelectObject(context, CreateSolidBrush(RGB(0x90, 0x90, 0x90)));
    Rectangle(context,
              0,
              0,
              page_setup.printable_area().width(),
              page_setup.printable_area().height());
    // Overlay area:
    gfx::Rect debug_overlay_area(page_setup.overlay_area());
    debug_overlay_area.Offset(-page_setup.printable_area().x(),
                              -page_setup.printable_area().y());
    SelectObject(context, CreateSolidBrush(RGB(0xb0, 0xb0, 0xb0)));
    DrawRect(context, debug_overlay_area);
    // Content area:
    gfx::Rect debug_content_area(page_setup.content_area());
    debug_content_area.Offset(-page_setup.printable_area().x(),
                              -page_setup.printable_area().y());
    SelectObject(context, CreateSolidBrush(RGB(0xd0, 0xd0, 0xd0)));
    DrawRect(context, debug_content_area);
#endif

    // Setup the matrix to translate and scale to the right place. Take in
    // account the actual shrinking factor.
    // Note that the printing output is relative to printable area of the page.
    // That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
    SimpleModifyWorldTransform(
        context,
        page_setup.content_area().x() - page_setup.printable_area().x(),
        page_setup.content_area().y() - page_setup.printable_area().y(),
        mutable_.shrink_factor);

    if (!page.native_metafile()->SafePlayback(context)) {
      NOTREACHED();
    }

    BOOL res = RestoreDC(context, saved_state);
    DCHECK_NE(res, 0);
  }

  // Print the header and footer.  Offset by printable area offset (see comment
  // above).
  SimpleModifyWorldTransform(
      context,
      -page_setup.printable_area().x(),
      -page_setup.printable_area().y(),
      1);
  int base_font_size = gfx::Font().height();
  int new_font_size = ConvertUnit(10,
                                  immutable_.settings_.desired_dpi,
                                  immutable_.settings_.dpi());
  DCHECK_GT(new_font_size, base_font_size);
  gfx::Font font(gfx::Font().DeriveFont(new_font_size - base_font_size));
  HGDIOBJ old_font = SelectObject(context, font.hfont());
  DCHECK(old_font != NULL);
  // We don't want a white square around the text ever if overflowing.
  SetBkMode(context, TRANSPARENT);
  PrintHeaderFooter(context, page, PageOverlays::LEFT, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::CENTER, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::RIGHT, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::LEFT, PageOverlays::BOTTOM,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::CENTER, PageOverlays::BOTTOM,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::RIGHT, PageOverlays::BOTTOM,
                    font);
  int res = RestoreDC(context, saved_state);
  DCHECK_NE(res, 0);
}

}  // namespace printing
