// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <map>

#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"

using WebKit::WebCursorInfo;

namespace {

// A process wide singleton that manages the usage of X cursors.
class CursorCache {
 public:
   CursorCache() {}
  ~CursorCache() {
    Display* display = base::MessagePumpForUI::GetDefaultXDisplay();
    for (std::map<int, Cursor>::iterator it =
        cache_.begin(); it != cache_.end(); ++it) {
      XFreeCursor(display, it->second);
    }
    cache_.clear();
  }

  Cursor GetCursor(int web_cursor_info_type) {
    Cursor cursor = gfx::kNullCursor;
    std::pair<std::map<int, Cursor>::iterator, bool> it = cache_.insert(
        std::make_pair(web_cursor_info_type, cursor));
    if (it.second) {
      cursor = XCreateFontCursor(base::MessagePumpForUI::GetDefaultXDisplay(),
          GetXCursorType(web_cursor_info_type));
      it.first->second = cursor;
    }
    return it.first->second;
  }

 private:
  int GetXCursorType(int web_cursor_info_type) {
    switch (web_cursor_info_type) {
      case WebCursorInfo::TypePointer:
        return XC_left_ptr;
      case WebCursorInfo::TypeCross:
        return XC_crosshair;
      case WebCursorInfo::TypeHand:
        return XC_hand2;
      case WebCursorInfo::TypeIBeam:
        return XC_xterm;
      case WebCursorInfo::TypeWait:
        return XC_watch;
      case WebCursorInfo::TypeHelp:
        return XC_question_arrow;
      case WebCursorInfo::TypeEastResize:
        return XC_right_side;
      case WebCursorInfo::TypeNorthResize:
        return XC_top_side;
      case WebCursorInfo::TypeNorthEastResize:
        return XC_top_right_corner;
      case WebCursorInfo::TypeNorthWestResize:
        return XC_top_left_corner;
      case WebCursorInfo::TypeSouthResize:
        return XC_bottom_side;
      case WebCursorInfo::TypeSouthEastResize:
        return XC_bottom_right_corner;
      case WebCursorInfo::TypeSouthWestResize:
        return XC_bottom_left_corner;
      case WebCursorInfo::TypeWestResize:
        return XC_left_side;
      case WebCursorInfo::TypeNorthSouthResize:
        return XC_sb_v_double_arrow;
      case WebCursorInfo::TypeEastWestResize:
        return XC_sb_h_double_arrow;
      case WebCursorInfo::TypeNorthEastSouthWestResize:
      case WebCursorInfo::TypeNorthWestSouthEastResize:
        // There isn't really a useful cursor available for these.
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeColumnResize:
        return XC_sb_h_double_arrow;
      case WebCursorInfo::TypeRowResize:
        return XC_sb_v_double_arrow;
      case WebCursorInfo::TypeMiddlePanning:
        return XC_fleur;
      case WebCursorInfo::TypeEastPanning:
        return XC_sb_right_arrow;
      case WebCursorInfo::TypeNorthPanning:
        return XC_sb_up_arrow;
      case WebCursorInfo::TypeNorthEastPanning:
        return XC_top_right_corner;
      case WebCursorInfo::TypeNorthWestPanning:
        return XC_top_left_corner;
      case WebCursorInfo::TypeSouthPanning:
        return XC_sb_down_arrow;
      case WebCursorInfo::TypeSouthEastPanning:
        return XC_bottom_right_corner;
      case WebCursorInfo::TypeSouthWestPanning:
        return XC_bottom_left_corner;
      case WebCursorInfo::TypeWestPanning:
        return XC_sb_left_arrow;
      case WebCursorInfo::TypeMove:
        return XC_fleur;
      case WebCursorInfo::TypeVerticalText:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeCell:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeContextMenu:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeAlias:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeProgress:
        return XC_watch;
      case WebCursorInfo::TypeNoDrop:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeCopy:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeNone:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeNotAllowed:
        NOTIMPLEMENTED();
        return XC_left_ptr;
      case WebCursorInfo::TypeZoomIn:
      case WebCursorInfo::TypeZoomOut:
      case WebCursorInfo::TypeGrab:
      case WebCursorInfo::TypeGrabbing:
      case WebCursorInfo::TypeCustom:
        NOTIMPLEMENTED();
        return XC_left_ptr;
    }
    NOTREACHED();
    return XC_left_ptr;
  }

  std::map<int, Cursor> cache_;

  DISALLOW_COPY_AND_ASSIGN(CursorCache);
};

}  // namespace

gfx::NativeCursor WebCursor::GetNativeCursor() {
  static CursorCache cache;
  return cache.GetCursor(type_);
}

void WebCursor::InitPlatformData() {
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
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
}
