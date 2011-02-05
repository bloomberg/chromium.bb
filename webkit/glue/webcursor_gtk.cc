// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/gfx/gtk_util.h"

using WebKit::WebCursorInfo;

namespace {

// webcursor_gtk_data.h is taken directly from WebKit's CursorGtk.h.
#include "webkit/glue/webcursor_gtk_data.h"

// This helper function is taken directly from WebKit's CursorGtk.cpp.
// It attempts to create a custom cursor from the data inlined in
// webcursor_gtk_data.h.
GdkCursor* GetInlineCustomCursor(CustomCursorType type) {
  static GdkCursor* CustomCursorsGdk[G_N_ELEMENTS(CustomCursors)];
  GdkCursor* cursor = CustomCursorsGdk[type];
  if (cursor)
    return cursor;
  const CustomCursor& custom = CustomCursors[type];
  cursor = gdk_cursor_new_from_name(gdk_display_get_default(), custom.name);
  if (!cursor) {
    const GdkColor fg = { 0, 0, 0, 0 };
    const GdkColor bg = { 65535, 65535, 65535, 65535 };
    GdkPixmap* source = gdk_bitmap_create_from_data(NULL, custom.bits,
                                                    32, 32);
    GdkPixmap* mask = gdk_bitmap_create_from_data(NULL, custom.mask_bits,
                                                  32, 32);
    cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg,
                                        custom.hot_x, custom.hot_y);
    g_object_unref(source);
    g_object_unref(mask);
  }
  CustomCursorsGdk[type] = cursor;
  return cursor;
}

// For GTK 2.16 and beyond, GDK_BLANK_CURSOR is available. Before, we have to
// use a custom cursor.
#if !GTK_CHECK_VERSION(2, 16, 0)
// Get/create a custom cursor which is invisible.
GdkCursor* GetInvisibleCustomCursor() {
  static GdkCursor* cursor = NULL;
  if (cursor)
    return cursor;
  const char bits[] = { 0 };
  const GdkColor color = { 0, 0, 0, 0 };
  GdkPixmap* bitmap = gdk_bitmap_create_from_data(NULL, bits, 1, 1);
  cursor = gdk_cursor_new_from_pixmap(bitmap, bitmap, &color, &color, 0, 0);
  g_object_unref(bitmap);
  return cursor;
}
#endif

}  // end anonymous namespace

int WebCursor::GetCursorType() const {
  // http://library.gnome.org/devel/gdk/2.12/gdk-Cursors.html has images
  // of the default X theme, but beware that the user's cursor theme can
  // change everything.
  switch (type_) {
    case WebCursorInfo::TypePointer:
      return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeCross:
      return GDK_CROSS;
    case WebCursorInfo::TypeHand:
      return GDK_HAND2;
    case WebCursorInfo::TypeIBeam:
      return GDK_XTERM;
    case WebCursorInfo::TypeWait:
      return GDK_WATCH;
    case WebCursorInfo::TypeHelp:
      return GDK_QUESTION_ARROW;
    case WebCursorInfo::TypeEastResize:
      return GDK_RIGHT_SIDE;
    case WebCursorInfo::TypeNorthResize:
      return GDK_TOP_SIDE;
    case WebCursorInfo::TypeNorthEastResize:
      return GDK_TOP_RIGHT_CORNER;
    case WebCursorInfo::TypeNorthWestResize:
      return GDK_TOP_LEFT_CORNER;
    case WebCursorInfo::TypeSouthResize:
      return GDK_BOTTOM_SIDE;
    case WebCursorInfo::TypeSouthEastResize:
      return GDK_BOTTOM_RIGHT_CORNER;
    case WebCursorInfo::TypeSouthWestResize:
      return GDK_BOTTOM_LEFT_CORNER;
    case WebCursorInfo::TypeWestResize:
      return GDK_LEFT_SIDE;
    case WebCursorInfo::TypeNorthSouthResize:
      return GDK_SB_V_DOUBLE_ARROW;
    case WebCursorInfo::TypeEastWestResize:
      return GDK_SB_H_DOUBLE_ARROW;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      // There isn't really a useful cursor available for these.
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeColumnResize:
      return GDK_SB_H_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case WebCursorInfo::TypeRowResize:
      return GDK_SB_V_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case WebCursorInfo::TypeMiddlePanning:
      return GDK_FLEUR;
    case WebCursorInfo::TypeEastPanning:
      return GDK_SB_RIGHT_ARROW;
    case WebCursorInfo::TypeNorthPanning:
      return GDK_SB_UP_ARROW;
    case WebCursorInfo::TypeNorthEastPanning:
      return GDK_TOP_RIGHT_CORNER;
    case WebCursorInfo::TypeNorthWestPanning:
      return GDK_TOP_LEFT_CORNER;
    case WebCursorInfo::TypeSouthPanning:
      return GDK_SB_DOWN_ARROW;
    case WebCursorInfo::TypeSouthEastPanning:
      return GDK_BOTTOM_RIGHT_CORNER;
    case WebCursorInfo::TypeSouthWestPanning:
      return GDK_BOTTOM_LEFT_CORNER;
    case WebCursorInfo::TypeWestPanning:
      return GDK_SB_LEFT_ARROW;
    case WebCursorInfo::TypeMove:
      return GDK_FLEUR;
    case WebCursorInfo::TypeVerticalText:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeCell:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeContextMenu:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeAlias:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeProgress:
      return GDK_WATCH;
    case WebCursorInfo::TypeNoDrop:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeCopy:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeNone:
// See comment above |GetInvisibleCustomCursor()|.
#if !GTK_CHECK_VERSION(2, 16, 0)
      return GDK_CURSOR_IS_PIXMAP;
#else
      return GDK_BLANK_CURSOR;
#endif
    case WebCursorInfo::TypeNotAllowed:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case WebCursorInfo::TypeZoomIn:
    case WebCursorInfo::TypeZoomOut:
    case WebCursorInfo::TypeCustom:
      return GDK_CURSOR_IS_PIXMAP;
  }
  NOTREACHED();
  return GDK_LAST_CURSOR;
}

gfx::NativeCursor WebCursor::GetNativeCursor() {
  int type = GetCursorType();
  if (type == GDK_CURSOR_IS_PIXMAP)
    return GetCustomCursor();
  return gfx::GetCursor(type);
}

GdkCursor* WebCursor::GetCustomCursor() {
  switch (type_) {
// See comment above |GetInvisibleCustomCursor()|.
#if !GTK_CHECK_VERSION(2, 16, 0)
    case WebCursorInfo::TypeNone:
      return GetInvisibleCustomCursor();
#endif
    case WebCursorInfo::TypeZoomIn:
      return GetInlineCustomCursor(CustomCursorZoomIn);
    case WebCursorInfo::TypeZoomOut:
      return GetInlineCustomCursor(CustomCursorZoomOut);
  }

  if (type_ != WebCursorInfo::TypeCustom) {
    NOTREACHED();
    return NULL;
  }

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   custom_size_.width(), custom_size_.height());
  bitmap.allocPixels();
  memcpy(bitmap.getAddr32(0, 0), custom_data_.data(), custom_data_.size());

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  GdkCursor* cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(),
                                                 pixbuf,
                                                 hotspot_.x(),
                                                 hotspot_.y());

  gdk_pixbuf_unref(pixbuf);

  if (unref_)
    gdk_cursor_unref(unref_);
  unref_ = cursor;
  return cursor;
}

void WebCursor::InitPlatformData() {
  unref_ = NULL;
  return;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(const Pickle* pickle, void** iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (unref_) {
    gdk_cursor_unref(unref_);
    unref_ = NULL;
  }
  return;
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (other.unref_)
    unref_ = gdk_cursor_ref(other.unref_);
  return;
}
