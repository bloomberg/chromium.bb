// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/mojom/cursor/cursor_struct_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/ws/public/mojom/cursor/cursor.mojom.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
ws::mojom::CursorType
EnumTraits<ws::mojom::CursorType, ui::CursorType>::ToMojom(
    ui::CursorType input) {
  switch (input) {
    case ui::CursorType::kNull:
      return ws::mojom::CursorType::kNull;
    case ui::CursorType::kPointer:
      return ws::mojom::CursorType::kPointer;
    case ui::CursorType::kCross:
      return ws::mojom::CursorType::kCross;
    case ui::CursorType::kHand:
      return ws::mojom::CursorType::kHand;
    case ui::CursorType::kIBeam:
      return ws::mojom::CursorType::kIBeam;
    case ui::CursorType::kWait:
      return ws::mojom::CursorType::kWait;
    case ui::CursorType::kHelp:
      return ws::mojom::CursorType::kHelp;
    case ui::CursorType::kEastResize:
      return ws::mojom::CursorType::kEastResize;
    case ui::CursorType::kNorthResize:
      return ws::mojom::CursorType::kNorthResize;
    case ui::CursorType::kNorthEastResize:
      return ws::mojom::CursorType::kNorthEastResize;
    case ui::CursorType::kNorthWestResize:
      return ws::mojom::CursorType::kNorthWestResize;
    case ui::CursorType::kSouthResize:
      return ws::mojom::CursorType::kSouthResize;
    case ui::CursorType::kSouthEastResize:
      return ws::mojom::CursorType::kSouthEastResize;
    case ui::CursorType::kSouthWestResize:
      return ws::mojom::CursorType::kSouthWestResize;
    case ui::CursorType::kWestResize:
      return ws::mojom::CursorType::kWestResize;
    case ui::CursorType::kNorthSouthResize:
      return ws::mojom::CursorType::kNorthSouthResize;
    case ui::CursorType::kEastWestResize:
      return ws::mojom::CursorType::kEastWestResize;
    case ui::CursorType::kNorthEastSouthWestResize:
      return ws::mojom::CursorType::kNorthEastSouthWestResize;
    case ui::CursorType::kNorthWestSouthEastResize:
      return ws::mojom::CursorType::kNorthWestSouthEastResize;
    case ui::CursorType::kColumnResize:
      return ws::mojom::CursorType::kColumnResize;
    case ui::CursorType::kRowResize:
      return ws::mojom::CursorType::kRowResize;
    case ui::CursorType::kMiddlePanning:
      return ws::mojom::CursorType::kMiddlePanning;
    case ui::CursorType::kEastPanning:
      return ws::mojom::CursorType::kEastPanning;
    case ui::CursorType::kNorthPanning:
      return ws::mojom::CursorType::kNorthPanning;
    case ui::CursorType::kNorthEastPanning:
      return ws::mojom::CursorType::kNorthEastPanning;
    case ui::CursorType::kNorthWestPanning:
      return ws::mojom::CursorType::kNorthWestPanning;
    case ui::CursorType::kSouthPanning:
      return ws::mojom::CursorType::kSouthPanning;
    case ui::CursorType::kSouthEastPanning:
      return ws::mojom::CursorType::kSouthEastPanning;
    case ui::CursorType::kSouthWestPanning:
      return ws::mojom::CursorType::kSouthWestPanning;
    case ui::CursorType::kWestPanning:
      return ws::mojom::CursorType::kWestPanning;
    case ui::CursorType::kMove:
      return ws::mojom::CursorType::kMove;
    case ui::CursorType::kVerticalText:
      return ws::mojom::CursorType::kVerticalText;
    case ui::CursorType::kCell:
      return ws::mojom::CursorType::kCell;
    case ui::CursorType::kContextMenu:
      return ws::mojom::CursorType::kContextMenu;
    case ui::CursorType::kAlias:
      return ws::mojom::CursorType::kAlias;
    case ui::CursorType::kProgress:
      return ws::mojom::CursorType::kProgress;
    case ui::CursorType::kNoDrop:
      return ws::mojom::CursorType::kNoDrop;
    case ui::CursorType::kCopy:
      return ws::mojom::CursorType::kCopy;
    case ui::CursorType::kNone:
      return ws::mojom::CursorType::kNone;
    case ui::CursorType::kNotAllowed:
      return ws::mojom::CursorType::kNotAllowed;
    case ui::CursorType::kZoomIn:
      return ws::mojom::CursorType::kZoomIn;
    case ui::CursorType::kZoomOut:
      return ws::mojom::CursorType::kZoomOut;
    case ui::CursorType::kGrab:
      return ws::mojom::CursorType::kGrab;
    case ui::CursorType::kGrabbing:
      return ws::mojom::CursorType::kGrabbing;
    case ui::CursorType::kCustom:
      return ws::mojom::CursorType::kCustom;
    case ui::CursorType::kDndNone:
    case ui::CursorType::kDndMove:
    case ui::CursorType::kDndCopy:
    case ui::CursorType::kDndLink:
      // The mojom version is the same as the restricted Webcursor constants;
      // don't allow system cursors to be transmitted.
      NOTREACHED();
      return ws::mojom::CursorType::kNull;
  }
  NOTREACHED();
  return ws::mojom::CursorType::kNull;
}

// static
bool EnumTraits<ws::mojom::CursorType, ui::CursorType>::FromMojom(
    ws::mojom::CursorType input,
    ui::CursorType* out) {
  switch (input) {
    case ws::mojom::CursorType::kNull:
      *out = ui::CursorType::kNull;
      return true;
    case ws::mojom::CursorType::kPointer:
      *out = ui::CursorType::kPointer;
      return true;
    case ws::mojom::CursorType::kCross:
      *out = ui::CursorType::kCross;
      return true;
    case ws::mojom::CursorType::kHand:
      *out = ui::CursorType::kHand;
      return true;
    case ws::mojom::CursorType::kIBeam:
      *out = ui::CursorType::kIBeam;
      return true;
    case ws::mojom::CursorType::kWait:
      *out = ui::CursorType::kWait;
      return true;
    case ws::mojom::CursorType::kHelp:
      *out = ui::CursorType::kHelp;
      return true;
    case ws::mojom::CursorType::kEastResize:
      *out = ui::CursorType::kEastResize;
      return true;
    case ws::mojom::CursorType::kNorthResize:
      *out = ui::CursorType::kNorthResize;
      return true;
    case ws::mojom::CursorType::kNorthEastResize:
      *out = ui::CursorType::kNorthEastResize;
      return true;
    case ws::mojom::CursorType::kNorthWestResize:
      *out = ui::CursorType::kNorthWestResize;
      return true;
    case ws::mojom::CursorType::kSouthResize:
      *out = ui::CursorType::kSouthResize;
      return true;
    case ws::mojom::CursorType::kSouthEastResize:
      *out = ui::CursorType::kSouthEastResize;
      return true;
    case ws::mojom::CursorType::kSouthWestResize:
      *out = ui::CursorType::kSouthWestResize;
      return true;
    case ws::mojom::CursorType::kWestResize:
      *out = ui::CursorType::kWestResize;
      return true;
    case ws::mojom::CursorType::kNorthSouthResize:
      *out = ui::CursorType::kNorthSouthResize;
      return true;
    case ws::mojom::CursorType::kEastWestResize:
      *out = ui::CursorType::kEastWestResize;
      return true;
    case ws::mojom::CursorType::kNorthEastSouthWestResize:
      *out = ui::CursorType::kNorthEastSouthWestResize;
      return true;
    case ws::mojom::CursorType::kNorthWestSouthEastResize:
      *out = ui::CursorType::kNorthWestSouthEastResize;
      return true;
    case ws::mojom::CursorType::kColumnResize:
      *out = ui::CursorType::kColumnResize;
      return true;
    case ws::mojom::CursorType::kRowResize:
      *out = ui::CursorType::kRowResize;
      return true;
    case ws::mojom::CursorType::kMiddlePanning:
      *out = ui::CursorType::kMiddlePanning;
      return true;
    case ws::mojom::CursorType::kEastPanning:
      *out = ui::CursorType::kEastPanning;
      return true;
    case ws::mojom::CursorType::kNorthPanning:
      *out = ui::CursorType::kNorthPanning;
      return true;
    case ws::mojom::CursorType::kNorthEastPanning:
      *out = ui::CursorType::kNorthEastPanning;
      return true;
    case ws::mojom::CursorType::kNorthWestPanning:
      *out = ui::CursorType::kNorthWestPanning;
      return true;
    case ws::mojom::CursorType::kSouthPanning:
      *out = ui::CursorType::kSouthPanning;
      return true;
    case ws::mojom::CursorType::kSouthEastPanning:
      *out = ui::CursorType::kSouthEastPanning;
      return true;
    case ws::mojom::CursorType::kSouthWestPanning:
      *out = ui::CursorType::kSouthWestPanning;
      return true;
    case ws::mojom::CursorType::kWestPanning:
      *out = ui::CursorType::kWestPanning;
      return true;
    case ws::mojom::CursorType::kMove:
      *out = ui::CursorType::kMove;
      return true;
    case ws::mojom::CursorType::kVerticalText:
      *out = ui::CursorType::kVerticalText;
      return true;
    case ws::mojom::CursorType::kCell:
      *out = ui::CursorType::kCell;
      return true;
    case ws::mojom::CursorType::kContextMenu:
      *out = ui::CursorType::kContextMenu;
      return true;
    case ws::mojom::CursorType::kAlias:
      *out = ui::CursorType::kAlias;
      return true;
    case ws::mojom::CursorType::kProgress:
      *out = ui::CursorType::kProgress;
      return true;
    case ws::mojom::CursorType::kNoDrop:
      *out = ui::CursorType::kNoDrop;
      return true;
    case ws::mojom::CursorType::kCopy:
      *out = ui::CursorType::kCopy;
      return true;
    case ws::mojom::CursorType::kNone:
      *out = ui::CursorType::kNone;
      return true;
    case ws::mojom::CursorType::kNotAllowed:
      *out = ui::CursorType::kNotAllowed;
      return true;
    case ws::mojom::CursorType::kZoomIn:
      *out = ui::CursorType::kZoomIn;
      return true;
    case ws::mojom::CursorType::kZoomOut:
      *out = ui::CursorType::kZoomOut;
      return true;
    case ws::mojom::CursorType::kGrab:
      *out = ui::CursorType::kGrab;
      return true;
    case ws::mojom::CursorType::kGrabbing:
      *out = ui::CursorType::kGrabbing;
      return true;
    case ws::mojom::CursorType::kCustom:
      *out = ui::CursorType::kCustom;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ws::mojom::CursorSize
EnumTraits<ws::mojom::CursorSize, ui::CursorSize>::ToMojom(
    ui::CursorSize input) {
  switch (input) {
    case ui::CursorSize::kNormal:
      return ws::mojom::CursorSize::kNormal;
    case ui::CursorSize::kLarge:
      return ws::mojom::CursorSize::kLarge;
  }

  NOTREACHED();
  return ws::mojom::CursorSize::kNormal;
}

// static
bool EnumTraits<ws::mojom::CursorSize, ui::CursorSize>::FromMojom(
    ws::mojom::CursorSize input,
    ui::CursorSize* out) {
  switch (input) {
    case ws::mojom::CursorSize::kNormal:
      *out = ui::CursorSize::kNormal;
      return true;
    case ws::mojom::CursorSize::kLarge:
      *out = ui::CursorSize::kLarge;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
const base::TimeDelta&
StructTraits<ws::mojom::CursorDataDataView, ui::CursorData>::frame_delay(
    const ui::CursorData& c) {
  return c.frame_delay();
}

// static
const gfx::Point&
StructTraits<ws::mojom::CursorDataDataView, ui::CursorData>::hotspot_in_pixels(
    const ui::CursorData& c) {
  return c.hotspot_in_pixels();
}

// static
const std::vector<SkBitmap>&
StructTraits<ws::mojom::CursorDataDataView, ui::CursorData>::cursor_frames(
    const ui::CursorData& c) {
  return c.cursor_frames();
}

// static
bool StructTraits<ws::mojom::CursorDataDataView, ui::CursorData>::Read(
    ws::mojom::CursorDataDataView data,
    ui::CursorData* out) {
  ui::CursorType type;
  if (!data.ReadCursorType(&type))
    return false;

  if (type != ui::CursorType::kCustom) {
    *out = ui::CursorData(type);
    return true;
  }

  gfx::Point hotspot_in_pixels;
  std::vector<SkBitmap> cursor_frames;
  base::TimeDelta frame_delay;

  if (!data.ReadHotspotInPixels(&hotspot_in_pixels) ||
      !data.ReadCursorFrames(&cursor_frames) || cursor_frames.empty() ||
      !data.ReadFrameDelay(&frame_delay)) {
    return false;
  }

  // Clamp the scale factor to a reasonable value. TODO(estade): do we even need
  // this field? It doesn't appear to be used anywhere and is a property of the
  // display, not the cursor.
  float scale_factor = data.scale_factor();
  if (scale_factor < 1.f || scale_factor > 3.f)
    return false;

  *out = ui::CursorData(hotspot_in_pixels, cursor_frames, scale_factor,
                        frame_delay);
  return true;
}

}  // namespace mojo
