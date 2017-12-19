// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursors_aura.h"

#include <stddef.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/gfx/icon_util.h"
#endif

namespace ui {
namespace {

struct HotPoint {
  int x;
  int y;
};

struct CursorData {
  CursorType id;
  int resource_id;
  HotPoint hot_1x;
  HotPoint hot_2x;
};

struct CursorSizeData {
  const CursorSize id;
  const CursorData* cursors;
  const int length;
  const CursorData* animated_cursors;
  const int animated_length;
};

const CursorData kNormalCursors[] = {
    {CursorType::kNull, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
    {CursorType::kPointer, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
    {CursorType::kNoDrop, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
    {CursorType::kNotAllowed, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
    {CursorType::kCopy, IDR_AURA_CURSOR_COPY, {9, 9}, {18, 18}},
    {CursorType::kHand, IDR_AURA_CURSOR_HAND, {9, 4}, {19, 8}},
    {CursorType::kMove, IDR_AURA_CURSOR_MOVE, {11, 11}, {23, 23}},
    {CursorType::kNorthEastResize,
     IDR_AURA_CURSOR_NORTH_EAST_RESIZE,
     {12, 11},
     {25, 23}},
    {CursorType::kSouthWestResize,
     IDR_AURA_CURSOR_SOUTH_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {CursorType::kSouthEastResize,
     IDR_AURA_CURSOR_SOUTH_EAST_RESIZE,
     {11, 11},
     {24, 23}},
    {CursorType::kNorthWestResize,
     IDR_AURA_CURSOR_NORTH_WEST_RESIZE,
     {11, 11},
     {24, 23}},
    {CursorType::kNorthResize,
     IDR_AURA_CURSOR_NORTH_RESIZE,
     {11, 12},
     {23, 23}},
    {CursorType::kSouthResize,
     IDR_AURA_CURSOR_SOUTH_RESIZE,
     {11, 12},
     {23, 23}},
    {CursorType::kEastResize, IDR_AURA_CURSOR_EAST_RESIZE, {12, 11}, {25, 23}},
    {CursorType::kWestResize, IDR_AURA_CURSOR_WEST_RESIZE, {12, 11}, {25, 23}},
    {CursorType::kIBeam, IDR_AURA_CURSOR_IBEAM, {12, 12}, {24, 25}},
    {CursorType::kAlias, IDR_AURA_CURSOR_ALIAS, {8, 6}, {15, 11}},
    {CursorType::kCell, IDR_AURA_CURSOR_CELL, {11, 11}, {24, 23}},
    {CursorType::kContextMenu, IDR_AURA_CURSOR_CONTEXT_MENU, {4, 4}, {8, 9}},
    {CursorType::kCross, IDR_AURA_CURSOR_CROSSHAIR, {12, 12}, {24, 24}},
    {CursorType::kHelp, IDR_AURA_CURSOR_HELP, {4, 4}, {8, 9}},
    {CursorType::kVerticalText,
     IDR_AURA_CURSOR_XTERM_HORIZ,
     {12, 11},
     {26, 23}},
    {CursorType::kZoomIn, IDR_AURA_CURSOR_ZOOM_IN, {10, 10}, {20, 20}},
    {CursorType::kZoomOut, IDR_AURA_CURSOR_ZOOM_OUT, {10, 10}, {20, 20}},
    {CursorType::kRowResize, IDR_AURA_CURSOR_ROW_RESIZE, {11, 12}, {23, 23}},
    {CursorType::kColumnResize, IDR_AURA_CURSOR_COL_RESIZE, {12, 11}, {25, 23}},
    {CursorType::kEastWestResize,
     IDR_AURA_CURSOR_EAST_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {CursorType::kNorthSouthResize,
     IDR_AURA_CURSOR_NORTH_SOUTH_RESIZE,
     {11, 12},
     {23, 23}},
    {CursorType::kNorthEastSouthWestResize,
     IDR_AURA_CURSOR_NORTH_EAST_SOUTH_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {CursorType::kNorthWestSouthEastResize,
     IDR_AURA_CURSOR_NORTH_WEST_SOUTH_EAST_RESIZE,
     {11, 11},
     {24, 23}},
    {CursorType::kGrab, IDR_AURA_CURSOR_GRAB, {8, 5}, {16, 10}},
    {CursorType::kGrabbing, IDR_AURA_CURSOR_GRABBING, {9, 9}, {18, 18}},
};

const CursorData kLargeCursors[] = {
    // The 2x hotspots should be double of the 1x, even though the cursors are
    // shown as same size as 1x (64x64), because in 2x dpi screen, the 1x large
    // cursor assets (64x64) are internally enlarged to the double size
    // (128x128)
    // by ResourceBundleImageSource.
    {CursorType::kNull, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
    {CursorType::kPointer, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
    {CursorType::kNoDrop, IDR_AURA_CURSOR_BIG_NO_DROP, {10, 10}, {20, 20}},
    {CursorType::kNotAllowed, IDR_AURA_CURSOR_BIG_NO_DROP, {10, 10}, {20, 20}},
    {CursorType::kCopy, IDR_AURA_CURSOR_BIG_COPY, {10, 10}, {20, 20}},
    {CursorType::kHand, IDR_AURA_CURSOR_BIG_HAND, {25, 7}, {50, 14}},
    {CursorType::kMove, IDR_AURA_CURSOR_BIG_MOVE, {32, 31}, {64, 62}},
    {CursorType::kNorthEastResize,
     IDR_AURA_CURSOR_BIG_NORTH_EAST_RESIZE,
     {31, 28},
     {62, 56}},
    {CursorType::kSouthWestResize,
     IDR_AURA_CURSOR_BIG_SOUTH_WEST_RESIZE,
     {31, 28},
     {62, 56}},
    {CursorType::kSouthEastResize,
     IDR_AURA_CURSOR_BIG_SOUTH_EAST_RESIZE,
     {28, 28},
     {56, 56}},
    {CursorType::kNorthWestResize,
     IDR_AURA_CURSOR_BIG_NORTH_WEST_RESIZE,
     {28, 28},
     {56, 56}},
    {CursorType::kNorthResize,
     IDR_AURA_CURSOR_BIG_NORTH_RESIZE,
     {29, 32},
     {58, 64}},
    {CursorType::kSouthResize,
     IDR_AURA_CURSOR_BIG_SOUTH_RESIZE,
     {29, 32},
     {58, 64}},
    {CursorType::kEastResize,
     IDR_AURA_CURSOR_BIG_EAST_RESIZE,
     {35, 29},
     {70, 58}},
    {CursorType::kWestResize,
     IDR_AURA_CURSOR_BIG_WEST_RESIZE,
     {35, 29},
     {70, 58}},
    {CursorType::kIBeam, IDR_AURA_CURSOR_BIG_IBEAM, {30, 32}, {60, 64}},
    {CursorType::kAlias, IDR_AURA_CURSOR_BIG_ALIAS, {19, 11}, {38, 22}},
    {CursorType::kCell, IDR_AURA_CURSOR_BIG_CELL, {30, 30}, {60, 60}},
    {CursorType::kContextMenu,
     IDR_AURA_CURSOR_BIG_CONTEXT_MENU,
     {11, 11},
     {22, 22}},
    {CursorType::kCross, IDR_AURA_CURSOR_BIG_CROSSHAIR, {30, 32}, {60, 64}},
    {CursorType::kHelp, IDR_AURA_CURSOR_BIG_HELP, {10, 11}, {20, 22}},
    {CursorType::kVerticalText,
     IDR_AURA_CURSOR_BIG_XTERM_HORIZ,
     {32, 30},
     {64, 60}},
    {CursorType::kZoomIn, IDR_AURA_CURSOR_BIG_ZOOM_IN, {25, 26}, {50, 52}},
    {CursorType::kZoomOut, IDR_AURA_CURSOR_BIG_ZOOM_OUT, {26, 26}, {52, 52}},
    {CursorType::kRowResize,
     IDR_AURA_CURSOR_BIG_ROW_RESIZE,
     {29, 32},
     {58, 64}},
    {CursorType::kColumnResize,
     IDR_AURA_CURSOR_BIG_COL_RESIZE,
     {35, 29},
     {70, 58}},
    {CursorType::kEastWestResize,
     IDR_AURA_CURSOR_BIG_EAST_WEST_RESIZE,
     {35, 29},
     {70, 58}},
    {CursorType::kNorthSouthResize,
     IDR_AURA_CURSOR_BIG_NORTH_SOUTH_RESIZE,
     {29, 32},
     {58, 64}},
    {CursorType::kNorthEastSouthWestResize,
     IDR_AURA_CURSOR_BIG_NORTH_EAST_SOUTH_WEST_RESIZE,
     {32, 30},
     {64, 60}},
    {CursorType::kNorthWestSouthEastResize,
     IDR_AURA_CURSOR_BIG_NORTH_WEST_SOUTH_EAST_RESIZE,
     {32, 31},
     {64, 62}},
    {CursorType::kGrab, IDR_AURA_CURSOR_BIG_GRAB, {21, 11}, {42, 22}},
    {CursorType::kGrabbing, IDR_AURA_CURSOR_BIG_GRABBING, {20, 12}, {40, 24}},
};

const CursorData kAnimatedCursors[] = {
    {CursorType::kWait, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
    {CursorType::kProgress, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
};

const CursorSizeData kCursorSizes[] = {
    {CursorSize::kNormal, kNormalCursors, arraysize(kNormalCursors),
     kAnimatedCursors, arraysize(kAnimatedCursors)},
    {CursorSize::kLarge, kLargeCursors, arraysize(kLargeCursors),
     // TODO(yoshiki): Replace animated cursors with big assets.
     // crbug.com/247254
     kAnimatedCursors, arraysize(kAnimatedCursors)},
};

const CursorSizeData* GetCursorSizeByType(CursorSize cursor_size) {
  for (size_t i = 0; i < arraysize(kCursorSizes); ++i) {
    if (kCursorSizes[i].id == cursor_size)
      return &kCursorSizes[i];
  }

  return NULL;
}

bool SearchTable(const CursorData* table,
                 size_t table_length,
                 CursorType id,
                 float scale_factor,
                 int* resource_id,
                 gfx::Point* point) {
  bool resource_2x_available =
      ResourceBundle::GetSharedInstance().GetMaxScaleFactor() ==
      SCALE_FACTOR_200P;
  for (size_t i = 0; i < table_length; ++i) {
    if (table[i].id == id) {
      *resource_id = table[i].resource_id;
      *point = scale_factor == 1.0f || !resource_2x_available ?
               gfx::Point(table[i].hot_1x.x, table[i].hot_1x.y) :
               gfx::Point(table[i].hot_2x.x, table[i].hot_2x.y);
      return true;
    }
  }

  return false;
}

}  // namespace

bool GetCursorDataFor(CursorSize cursor_size,
                      CursorType id,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point) {
  const CursorSizeData* cursor_set = GetCursorSizeByType(cursor_size);
  if (cursor_set &&
      SearchTable(cursor_set->cursors,
                  cursor_set->length,
                  id, scale_factor, resource_id, point)) {
      return true;
  }

  // Falls back to the default cursor set.
  cursor_set = GetCursorSizeByType(ui::CursorSize::kNormal);
  DCHECK(cursor_set);
  return SearchTable(cursor_set->cursors,
                     cursor_set->length,
                     id, scale_factor, resource_id, point);
}

bool GetAnimatedCursorDataFor(CursorSize cursor_size,
                              CursorType id,
                              float scale_factor,
                              int* resource_id,
                              gfx::Point* point) {
  const CursorSizeData* cursor_set = GetCursorSizeByType(cursor_size);
  if (cursor_set &&
      SearchTable(cursor_set->animated_cursors,
                  cursor_set->animated_length,
                  id, scale_factor, resource_id, point)) {
    return true;
  }

  // Falls back to the default cursor set.
  cursor_set = GetCursorSizeByType(ui::CursorSize::kNormal);
  DCHECK(cursor_set);
  return SearchTable(cursor_set->animated_cursors,
                     cursor_set->animated_length,
                     id, scale_factor, resource_id, point);
}

bool GetCursorBitmap(const Cursor& cursor,
                     SkBitmap* bitmap,
                     gfx::Point* point) {
  DCHECK(bitmap && point);
#if defined(OS_WIN)
  Cursor cursor_copy = cursor;
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor_copy);
  const std::unique_ptr<SkBitmap> cursor_bitmap(
      IconUtil::CreateSkBitmapFromHICON(cursor_copy.platform()));
  *point = IconUtil::GetHotSpotFromHICON(cursor_copy.platform());
#else
  int resource_id;
  if (!GetCursorDataFor(ui::CursorSize::kNormal, cursor.native_type(),
                        cursor.device_scale_factor(), &resource_id, point)) {
    return false;
  }

  const SkBitmap* cursor_bitmap = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(resource_id)->bitmap();
#endif
  if (!cursor_bitmap)
    return false;
  *bitmap = *cursor_bitmap;
  return true;
}

}  // namespace ui
