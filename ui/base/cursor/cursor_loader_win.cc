// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

namespace ui {

#if defined(USE_AURA)

namespace {

const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor.native_type()) {
    case kCursorNull:
      return IDC_ARROW;
    case kCursorPointer:
      return IDC_ARROW;
    case kCursorCross:
      return IDC_CROSS;
    case kCursorHand:
      return IDC_HAND;
    case kCursorIBeam:
      return IDC_IBEAM;
    case kCursorWait:
      return IDC_WAIT;
    case kCursorHelp:
      return IDC_HELP;
    case kCursorEastResize:
      return IDC_SIZEWE;
    case kCursorNorthResize:
      return IDC_SIZENS;
    case kCursorNorthEastResize:
      return IDC_SIZENESW;
    case kCursorNorthWestResize:
      return IDC_SIZENWSE;
    case kCursorSouthResize:
      return IDC_SIZENS;
    case kCursorSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorSouthWestResize:
      return IDC_SIZENESW;
    case kCursorWestResize:
      return IDC_SIZEWE;
    case kCursorNorthSouthResize:
      return IDC_SIZENS;
    case kCursorEastWestResize:
      return IDC_SIZEWE;
    case kCursorNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case kCursorNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorMove:
      return IDC_SIZEALL;
    case kCursorProgress:
      return IDC_APPSTARTING;
    case kCursorNoDrop:
      return IDC_NO;
    case kCursorNotAllowed:
      return IDC_NO;
    case kCursorColumnResize:
    case kCursorRowResize:
    case kCursorMiddlePanning:
    case kCursorEastPanning:
    case kCursorNorthPanning:
    case kCursorNorthEastPanning:
    case kCursorNorthWestPanning:
    case kCursorSouthPanning:
    case kCursorSouthEastPanning:
    case kCursorSouthWestPanning:
    case kCursorWestPanning:
    case kCursorVerticalText:
    case kCursorCell:
    case kCursorContextMenu:
    case kCursorAlias:
    case kCursorCopy:
    case kCursorNone:
    case kCursorZoomIn:
    case kCursorZoomOut:
    case kCursorGrab:
    case kCursorGrabbing:
    case kCursorCustom:
      // TODO(jamescook): Should we use WebKit glue resources for these?
      // Or migrate those resources to someplace ui/aura can share?
      NOTIMPLEMENTED();
      return IDC_ARROW;
    default:
      NOTREACHED();
      return IDC_ARROW;
  }
}

}  // namespace

#endif

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderWin;
}

CursorLoaderWin::CursorLoaderWin() {
}

CursorLoaderWin::~CursorLoaderWin() {
}

void CursorLoaderWin::LoadImageCursor(int id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::LoadAnimatedCursor(int id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::UnloadAll() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
#if defined(USE_AURA)
  if (cursor->native_type() != kCursorCustom) {
    const wchar_t* cursor_id = GetCursorId(*cursor);

    // TODO(jamescook): Support for non-system cursors will require finding
    // the appropriate module to pass to LoadCursor().
    cursor->SetPlatformCursor(LoadCursor(NULL, cursor_id));
  }
#endif
}

}  // namespace ui
