// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_TYPE_H_
#define UI_BASE_CURSOR_CURSOR_TYPE_H_

namespace ui {

enum class CursorType {
  // Equivalent to a NULL HCURSOR on Windows.
  kNull = 0,

  // These cursors mirror WebKit cursors from WebCursorInfo, but are replicated
  // here so we don't introduce a WebKit dependency.
  // TODO(crbug.com/969904): Merge this with WebCursorInfo
  kPointer = 1,
  kCross = 2,
  kHand = 3,
  kIBeam = 4,
  kWait = 5,
  kHelp = 6,
  kEastResize = 7,
  kNorthResize = 8,
  kNorthEastResize = 9,
  kNorthWestResize = 10,
  kSouthResize = 11,
  kSouthEastResize = 12,
  kSouthWestResize = 13,
  kWestResize = 14,
  kNorthSouthResize = 15,
  kEastWestResize = 16,
  kNorthEastSouthWestResize = 17,
  kNorthWestSouthEastResize = 18,
  kColumnResize = 19,
  kRowResize = 20,
  kMiddlePanning = 21,
  kEastPanning = 22,
  kNorthPanning = 23,
  kNorthEastPanning = 24,
  kNorthWestPanning = 25,
  kSouthPanning = 26,
  kSouthEastPanning = 27,
  kSouthWestPanning = 28,
  kWestPanning = 29,
  kMove = 30,
  kVerticalText = 31,
  kCell = 32,
  kContextMenu = 33,
  kAlias = 34,
  kProgress = 35,
  kNoDrop = 36,
  kCopy = 37,
  kNone = 38,
  kNotAllowed = 39,
  kZoomIn = 40,
  kZoomOut = 41,
  kGrab = 42,
  kGrabbing = 43,
  kMiddlePanningVertical = 44,
  kMiddlePanningHorizontal = 45,
  kCustom = 46,

  // These additional drag and drop cursors are not listed in WebCursorInfo.
  kDndNone = 47,
  kDndMove = 48,
  kDndCopy = 49,
  kDndLink = 50,
};

enum class CursorSize { kNormal, kLarge };

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_TYPE_H_
