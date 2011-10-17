// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/aura/cursor.h"

using WebKit::WebCursorInfo;

gfx::NativeCursor WebCursor::GetNativeCursor() {
  switch (type_) {
    case WebCursorInfo::TypePointer:
      return aura::kCursorPointer;
    case WebCursorInfo::TypeCross:
      return aura::kCursorCross;
    case WebCursorInfo::TypeHand:
      return aura::kCursorHand;
    case WebCursorInfo::TypeIBeam:
      return aura::kCursorIBeam;
    case WebCursorInfo::TypeWait:
      return aura::kCursorWait;
    case WebCursorInfo::TypeHelp:
      return aura::kCursorHelp;
    case WebCursorInfo::TypeEastResize:
      return aura::kCursorEastResize;
    case WebCursorInfo::TypeNorthResize:
      return aura::kCursorNorthResize;
    case WebCursorInfo::TypeNorthEastResize:
      return aura::kCursorNorthEastResize;
    case WebCursorInfo::TypeNorthWestResize:
      return aura::kCursorNorthWestResize;
    case WebCursorInfo::TypeSouthResize:
      return aura::kCursorSouthResize;
    case WebCursorInfo::TypeSouthEastResize:
      return aura::kCursorSouthEastResize;
    case WebCursorInfo::TypeSouthWestResize:
      return aura::kCursorSouthWestResize;
    case WebCursorInfo::TypeWestResize:
      return aura::kCursorWestResize;
    case WebCursorInfo::TypeNorthSouthResize:
      return aura::kCursorNorthSouthResize;
    case WebCursorInfo::TypeEastWestResize:
      return aura::kCursorEastWestResize;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return aura::kCursorNorthEastSouthWestResize;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return aura::kCursorNorthWestSouthEastResize;
    case WebCursorInfo::TypeColumnResize:
      return aura::kCursorColumnResize;
    case WebCursorInfo::TypeRowResize:
      return aura::kCursorRowResize;
    case WebCursorInfo::TypeMiddlePanning:
      return aura::kCursorMiddlePanning;
    case WebCursorInfo::TypeEastPanning:
      return aura::kCursorEastPanning;
    case WebCursorInfo::TypeNorthPanning:
      return aura::kCursorNorthPanning;
    case WebCursorInfo::TypeNorthEastPanning:
      return aura::kCursorNorthEastPanning;
    case WebCursorInfo::TypeNorthWestPanning:
      return aura::kCursorNorthWestPanning;
    case WebCursorInfo::TypeSouthPanning:
      return aura::kCursorSouthPanning;
    case WebCursorInfo::TypeSouthEastPanning:
      return aura::kCursorSouthEastPanning;
    case WebCursorInfo::TypeSouthWestPanning:
      return aura::kCursorSouthWestPanning;
    case WebCursorInfo::TypeWestPanning:
      return aura::kCursorWestPanning;
    case WebCursorInfo::TypeMove:
      return aura::kCursorMove;
    case WebCursorInfo::TypeVerticalText:
      return aura::kCursorVerticalText;
    case WebCursorInfo::TypeCell:
      return aura::kCursorCell;
    case WebCursorInfo::TypeContextMenu:
      return aura::kCursorContextMenu;
    case WebCursorInfo::TypeAlias:
      return aura::kCursorAlias;
    case WebCursorInfo::TypeProgress:
      return aura::kCursorProgress;
    case WebCursorInfo::TypeNoDrop:
      return aura::kCursorNoDrop;
    case WebCursorInfo::TypeCopy:
      return aura::kCursorCopy;
    case WebCursorInfo::TypeNone:
      return aura::kCursorNone;
    case WebCursorInfo::TypeNotAllowed:
      return aura::kCursorNotAllowed;
    case WebCursorInfo::TypeZoomIn:
      return aura::kCursorZoomIn;
    case WebCursorInfo::TypeZoomOut:
      return aura::kCursorZoomOut;
    case WebCursorInfo::TypeGrab:
      return aura::kCursorGrab;
    case WebCursorInfo::TypeGrabbing:
      return aura::kCursorGrabbing;
    case WebCursorInfo::TypeCustom:
      return aura::kCursorCustom;
    default:
      NOTREACHED();
      return gfx::kNullCursor;
  }
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
