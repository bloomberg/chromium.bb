// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/accelerated_surface_transformer_win.h"

#include <vector>

#include "accelerated_surface_transformer_win_hlsl_compiled.h"
#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/surface/d3d9_utils_win.h"
#include "ui/surface/surface_export.h"

using base::win::ScopedComPtr;
using std::vector;

namespace d3d_utils = ui_surface_d3d9_utils;

namespace {

struct Vertex {
  float x, y, z, w;
  float u, v;
};

const static D3DVERTEXELEMENT9 g_vertexElements[] = {
  { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0 },
  { 0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },
  D3DDECL_END()
};

// Calculate the number necessary to transform |src_subrect| into |dst_size|
// by repeating downsampling of the image of |src_subrect| by a factor no more
// than 2.
int GetResampleCount(const gfx::Rect& src_subrect,
                     const gfx::Size& dst_size,
                     const gfx::Size& back_buffer_size) {
  // At least one copy is required, since the back buffer itself is not
  // lockable.
  int min_resample_count = 1;
  int width_count = 0;
  int width = src_subrect.width();
  while (width > dst_size.width()) {
    ++width_count;
    width >>= 1;
  }
  int height_count = 0;
  int height = src_subrect.height();
  while (height > dst_size.height()) {
    ++height_count;
    height >>= 1;
  }
  return std::max(std::max(width_count, height_count),
                  min_resample_count);
}

// Returns half the size of |size| no smaller than |min_size|.
gfx::Size GetHalfSizeNoLessThan(const gfx::Size& size,
                                const gfx::Size& min_size) {
  return gfx::Size(std::max(min_size.width(), size.width() / 2),
                   std::max(min_size.height(), size.height() / 2));
}

gfx::Size GetSize(IDirect3DSurface9* surface) {
  D3DSURFACE_DESC surface_description;
  HRESULT hr = surface->GetDesc(&surface_description);
  if (FAILED(hr))
    return gfx::Size(0, 0);
  return gfx::Size(surface_description.Width, surface_description.Height);
}

}  // namespace


AcceleratedSurfaceTransformer::AcceleratedSurfaceTransformer() {}

bool AcceleratedSurfaceTransformer::Init(IDirect3DDevice9* device) {
  device_ = device;
  if (!InitShaderCombo(
          ui_surface::AcceleratedSurfaceTransformerWinHLSL::kVsOneTexture,
          ui_surface::AcceleratedSurfaceTransformerWinHLSL::kPsOneTexture,
          SIMPLE_TEXTURE)) {
    ReleaseAll();
    return false;
  }

  base::win::ScopedComPtr<IDirect3DVertexDeclaration9> vertex_declaration;
  HRESULT hr = device_->CreateVertexDeclaration(g_vertexElements,
                                                vertex_declaration.Receive());
  if (!SUCCEEDED(hr)) {
    ReleaseAll();
    return false;
  }
  device_->SetVertexDeclaration(vertex_declaration);

  return true;
}

bool AcceleratedSurfaceTransformer::InitShaderCombo(
    const BYTE vertex_shader_instructions[],
    const BYTE pixel_shader_instructions[],
    ShaderCombo shader_combo_name) {
  HRESULT hr = device_->CreateVertexShader(
      reinterpret_cast<const DWORD*>(vertex_shader_instructions),
      vertex_shaders_[shader_combo_name].Receive());

  if (FAILED(hr))
    return false;

  hr = device_->CreatePixelShader(
      reinterpret_cast<const DWORD*>(pixel_shader_instructions),
      pixel_shaders_[shader_combo_name].Receive());

  return SUCCEEDED(hr);
}


void AcceleratedSurfaceTransformer::ReleaseAll() {
  for (int i = 0; i < NUM_SHADERS; i++) {
    vertex_shaders_[i] = NULL;
    vertex_shaders_[i] = NULL;
  }
  device_ = NULL;
}
void AcceleratedSurfaceTransformer::DetachAll() {
  for (int i = 0; i < NUM_SHADERS; i++) {
    vertex_shaders_[i].Detach();
    vertex_shaders_[i].Detach();
  }
  device_.Detach();
}

// Draw a textured quad to a surface.
bool AcceleratedSurfaceTransformer::CopyInverted(
    IDirect3DTexture9* src_texture,
    IDirect3DSurface9* dst_surface,
    const gfx::Size& dst_size) {
  base::win::ScopedComPtr<IDirect3DSurface9> default_color_target;
  device()->GetRenderTarget(0, default_color_target.Receive());

  if (!SetShaderCombo(SIMPLE_TEXTURE))
    return false;

  device()->SetRenderTarget(0, dst_surface);
  device()->SetTexture(0, src_texture);

  D3DVIEWPORT9 viewport = {
    0, 0,
    dst_size.width(), dst_size.height(),
    0, 1
  };
  device()->SetViewport(&viewport);

  float halfPixelX = -1.0f / dst_size.width();
  float halfPixelY = 1.0f / dst_size.height();
  Vertex vertices[] = {
    { halfPixelX - 1, halfPixelY + 1, 0.5f, 1, 0, 1 },
    { halfPixelX + 1, halfPixelY + 1, 0.5f, 1, 1, 1 },
    { halfPixelX + 1, halfPixelY - 1, 0.5f, 1, 1, 0 },
    { halfPixelX - 1, halfPixelY - 1, 0.5f, 1, 0, 0 }
  };

  device()->BeginScene();
  device()->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,
                            2,
                            vertices,
                            sizeof(vertices[0]));
  device()->EndScene();

  // Clear surface references.
  device()->SetRenderTarget(0, default_color_target);
  device()->SetTexture(0, NULL);
  return true;
}

// Resize an RGB surface using repeated linear interpolation.
bool AcceleratedSurfaceTransformer::ResizeBilinear(
    IDirect3DSurface9* src_surface,
    const gfx::Rect& src_subrect,
    IDirect3DSurface9* dst_surface) {
  gfx::Size src_size = GetSize(src_surface);
  gfx::Size dst_size = GetSize(dst_surface);

  if (src_size.IsEmpty() || dst_size.IsEmpty())
    return false;

  HRESULT hr = S_OK;
  // Set up intermediate buffers needed for downsampling.
  const int resample_count =
      GetResampleCount(src_subrect, dst_size, src_size);
  base::win::ScopedComPtr<IDirect3DSurface9> temp_buffer[2];
  const gfx::Size half_size =
      GetHalfSizeNoLessThan(src_subrect.size(), dst_size);
  if (resample_count > 1) {
    TRACE_EVENT0("gpu", "CreateTemporarySurface");
    if (!d3d_utils::CreateTemporaryLockableSurface(device(),
                                                   half_size,
                                                   temp_buffer[0].Receive()))
      return false;
  }
  if (resample_count > 2) {
    TRACE_EVENT0("gpu", "CreateTemporarySurface");
    const gfx::Size quarter_size = GetHalfSizeNoLessThan(half_size, dst_size);
    if (!d3d_utils::CreateTemporaryLockableSurface(device(),
                                                   quarter_size,
                                                   temp_buffer[1].Receive()))
      return false;
  }

  // Repeat downsampling the surface until its size becomes identical to
  // |dst_size|. We keep the factor of each downsampling no more than two
  // because using a factor more than two can introduce aliasing.
  RECT read_rect = src_subrect.ToRECT();
  gfx::Size write_size = half_size;
  int read_buffer_index = 1;
  int write_buffer_index = 0;
  for (int i = 0; i < resample_count; ++i) {
    TRACE_EVENT0("gpu", "StretchRect");
    IDirect3DSurface9* read_buffer =
        (i == 0) ? src_surface : temp_buffer[read_buffer_index];
    IDirect3DSurface9* write_buffer =
        (i == resample_count - 1) ? dst_surface :
                                    temp_buffer[write_buffer_index];
    RECT write_rect = gfx::Rect(write_size).ToRECT();
    hr = device()->StretchRect(read_buffer,
                               &read_rect,
                               write_buffer,
                               &write_rect,
                               D3DTEXF_LINEAR);

    if (FAILED(hr))
      return false;
    read_rect = write_rect;
    write_size = GetHalfSizeNoLessThan(write_size, dst_size);
    std::swap(read_buffer_index, write_buffer_index);
  }

  return true;
}

IDirect3DDevice9* AcceleratedSurfaceTransformer::device() {
  return device_;
}

bool AcceleratedSurfaceTransformer::SetShaderCombo(ShaderCombo combo) {
  HRESULT hr = device()->SetVertexShader(vertex_shaders_[combo]);
  if (!SUCCEEDED(hr))
    return false;
  hr = device()->SetPixelShader(pixel_shaders_[combo]);
  if (!SUCCEEDED(hr))
    return false;
  return true;
}