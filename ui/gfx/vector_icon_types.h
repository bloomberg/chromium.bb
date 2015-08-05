// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides defines needed by PaintVectorIcon and is implemented
// by the generated file vector_icons.cc.

#ifndef UI_GFX_VECTOR_ICON_TYPES_H_
#define UI_GFX_VECTOR_ICON_TYPES_H_

#include "third_party/skia/include/core/SkScalar.h"

namespace gfx {

enum class VectorIconId;

// The size of a single side of the square canvas to which path coordinates
// are relative, in device independent pixels.
const int kReferenceSizeDip = 48;

// A command to Skia.
enum CommandType {
  // A new <path> element. For the first path, this is assumed.
  NEW_PATH,
  // Sets the color for the current path.
  PATH_COLOR_ARGB,
  // These correspond to pathing commands.
  MOVE_TO,
  R_MOVE_TO,
  LINE_TO,
  R_LINE_TO,
  H_LINE_TO,
  R_H_LINE_TO,
  V_LINE_TO,
  R_V_LINE_TO,
  CUBIC_TO,
  R_CUBIC_TO,
  CIRCLE,
  CLOSE,
  // Sets the dimensions of the canvas in dip. (Default is kReferenceSizeDip.)
  CANVAS_DIMENSIONS,
  // Marks the end of the list of commands.
  END
};

// A POD that describes either a path command or an argument for it.
struct PathElement {
  PathElement(CommandType value) : type(value) {}
  PathElement(SkScalar value) : arg(value) {}

  union {
    CommandType type;
    SkScalar arg;
  };
};

// Returns an array of path commands and arguments, terminated by END.
const PathElement* GetPathForVectorIcon(VectorIconId id);

}  // namespace gfx

#endif  // UI_GFX_VECTOR_ICON_TYPES_H_
