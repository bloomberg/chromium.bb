// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BRUSH_H_
#define UI_GFX_BRUSH_H_
#pragma once

namespace gfx {

// An object that encapsulates a platform native brush.
// Subclasses handle memory management of the underlying native brush.
class Brush {
 public:
  Brush() {}
  virtual ~Brush() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Brush);
};

}  // namespace gfx

#endif  // UI_GFX_BRUSH_H_
