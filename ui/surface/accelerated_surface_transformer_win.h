// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_ACCELERATED_SURFACE_TRANSFORMER_WIN_H_
#define UI_SURFACE_ACCELERATED_SURFACE_TRANSFORMER_WIN_H_

#include <d3d9.h>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/surface/surface_export.h"

namespace gfx {
class Size;
class Rect;
}  // namespace gfx

// Provides useful image filtering operations that are implemented
// efficiently on DirectX9-class hardware using fragment programs.
class SURFACE_EXPORT AcceleratedSurfaceTransformer {
 public:
  // Constructs an uninitialized surface transformer. Call Init() before
  // using the resulting object.
  AcceleratedSurfaceTransformer();

  // Init() initializes the transformer to operate on a device. This must be
  // called before any other method of this class, and it must be called
  // again after ReleaseAll() or DetachAll() before the class is used.
  //
  // Returns true if successful.
  bool Init(IDirect3DDevice9* device);

  // ReleaseAll() releases all direct3d resource references.
  void ReleaseAll();

  // DetachAll() leaks all direct3d resource references. This exists in order to
  // work around particular driver bugs, and should only be called at shutdown.
  // TODO(ncarter): Update the leak expectations before checkin.
  void DetachAll();

  // Draw a textured quad to a surface, flipping orientation in the y direction.
  bool CopyInverted(
      IDirect3DTexture9* src_texture,
      IDirect3DSurface9* dst_surface,
      const gfx::Size& dst_size);

  // Resize a surface using repeated bilinear interpolation.
  bool ResizeBilinear(
    IDirect3DSurface9* src_surface,
    const gfx::Rect& src_subrect,
    IDirect3DSurface9* dst_surface);

 private:
  enum ShaderCombo {
    SIMPLE_TEXTURE,
    NUM_SHADERS
  };

  // Set the active vertex and pixel shader combination.
  bool SetShaderCombo(ShaderCombo combo);

  // Intitializes a vertex and pixel shader combination from compiled bytecode.
  bool InitShaderCombo(const BYTE vertex_shader_instructions[],
                       const BYTE pixel_shader_instructions[],
                       ShaderCombo shader_combo_name);

  IDirect3DDevice9* device();

  base::win::ScopedComPtr<IDirect3DDevice9> device_;
  base::win::ScopedComPtr<IDirect3DVertexShader9> vertex_shaders_[NUM_SHADERS];
  base::win::ScopedComPtr<IDirect3DPixelShader9> pixel_shaders_[NUM_SHADERS];
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceTransformer);
};

#endif  // UI_SURFACE_ACCELERATED_SURFACE_TRANSFORMER_WIN_H_