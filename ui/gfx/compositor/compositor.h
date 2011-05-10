// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;
namespace gfx {
class Point;
class Size;
}

namespace ui {
class Transform;

#if !defined(COMPOSITOR_2)
typedef unsigned int TextureID;

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy. The initial implementation uses GL for this purpose.
// Future CLs will adapt this to ongoing Skia development.
class Compositor : public base::RefCounted<Compositor> {
 public:
  // Create a compositor from the provided handle.
  static Compositor* Create(gfx::AcceleratedWidget widget);

  // Notifies the compositor that compositing is about to start.
  virtual void NotifyStart() = 0;

  // Notifies the compositor that compositing is complete.
  virtual void NotifyEnd() = 0;

  // Draws the given texture with the given transform.
  virtual void DrawTextureWithTransform(TextureID txt,
                                        const ui::Transform& transform) = 0;

  // Save the current transformation that can be restored with RestoreTransform.
  virtual void SaveTransform() = 0;

  // Restore a previously saved transformation using SaveTransform.
  virtual void RestoreTransform() = 0;

 protected:
  virtual ~Compositor() {}

 private:
  friend class base::RefCounted<Compositor>;
};

#else
// Textures are created by a Compositor for managing an accelerated view.
// Any time a View with a texture needs to redraw itself it invokes SetBitmap().
// When the view is ready to be drawn Draw() is invoked.
//
// Texture is really a proxy to the gpu. Texture does not itself keep a copy of
// the bitmap.
//
// Views own the Texture.
class Texture {
 public:
  virtual ~Texture() {}

  // Sets the bitmap of this texture. The bitmaps origin is at |origin|.
  // |overall_size| gives the total size of texture.
  virtual void SetBitmap(const SkBitmap& bitmap,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) = 0;

  // Draws the texture.
  virtual void Draw(const ui::Transform& transform) = 0;
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class Compositor : public base::RefCounted<Compositor> {
 public:
  // Create a compositor from the provided handle.
  static Compositor* Create(gfx::AcceleratedWidget widget);

  // Creates a new texture. The caller owns the returned object.
  virtual Texture* CreateTexture() = 0;

  // Notifies the compositor that compositing is about to start.
  virtual void NotifyStart() = 0;

  // Notifies the compositor that compositing is complete.
  virtual void NotifyEnd() = 0;

 protected:
  virtual ~Compositor() {}

 private:
  friend class base::RefCounted<Compositor>;
};

#endif  // COMPOSITOR_2
}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_H_
