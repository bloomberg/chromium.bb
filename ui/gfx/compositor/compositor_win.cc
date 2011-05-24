// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include <algorithm>
#include <d3dx10.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/win/scoped_comptr.h"
#include "grit/gfx_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

// TODO(sky): this is a hack, figure out real error handling.
#define RETURN_IF_FAILED(error) \
  if (error != S_OK) { \
    this->Errored(error); \
    VLOG(1) << "D3D failed" << error; \
    return; \
  }

using base::win::ScopedComPtr;

namespace ui {

namespace {

class ViewTexture;

// ViewTexture talks to its host by way of this interface.
class ViewTextureHost {
 public:
  // Invoked to update the perspective needed by this texture. |transform| is
  // the transform for the texture, and |size| the size of the texture.
  virtual void UpdatePerspective(const ui::Transform& transform,
                                 const gfx::Size& size) = 0;

  // Returns the overall size of the compositor.
  virtual const gfx::Size& GetHostSize() = 0;

 protected:
  virtual ~ViewTextureHost() {}
};

// D3D 10 Texture implementation. Creates a quad representing the view and
// a texture with the bitmap data. The quad has an origin of 0,0,0 with a size
// matching that of |SetBitmap|.
class ViewTexture : public Texture {
 public:
  ViewTexture(ViewTextureHost* host,
              ID3D10Device* device,
              ID3D10Effect* effect);

  ~ViewTexture();

  void Init();

  // Texture:
  virtual void SetBitmap(const SkBitmap& bitmap,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;
  virtual void Draw(const ui::Transform& transform) OVERRIDE;

 private:
  struct Vertex {
    D3DXVECTOR3 position;
    D3DXVECTOR2 texture_offset;
  };

  void Errored(HRESULT result);

  void ConvertBitmapToD3DData(const SkBitmap& bitmap,
                              scoped_array<uint32>* converted_data);

  void CreateVertexBuffer(const gfx::Size& size);

  // TODO: this should be shared among all textures.
  void CreateIndexBuffer();

  ViewTextureHost* host_;

  // Size of the corresponding View.
  gfx::Size view_size_;

  ScopedComPtr<ID3D10Device> device_;
  ScopedComPtr<ID3D10Effect, NULL> effect_;
  ScopedComPtr<ID3D10Texture2D> texture_;
  ScopedComPtr<ID3D10ShaderResourceView> shader_view_;
  ScopedComPtr<ID3D10Buffer> vertex_buffer_;
  ScopedComPtr<ID3D10Buffer> index_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ViewTexture);
};

ViewTexture::ViewTexture(ViewTextureHost* host,
                         ID3D10Device* device,
                         ID3D10Effect* effect)
    : host_(host),
      device_(device),
      effect_(effect) {
}

ViewTexture::~ViewTexture() {
}

void ViewTexture::Init() {
  CreateIndexBuffer();
}

void ViewTexture::SetBitmap(const SkBitmap& bitmap,
                            const gfx::Point& origin,
                            const gfx::Size& overall_size) {
  if (view_size_ != overall_size)
    CreateVertexBuffer(overall_size);
  view_size_ = overall_size;

  scoped_array<uint32> converted_data;
  ConvertBitmapToD3DData(bitmap, &converted_data);
  if (gfx::Size(bitmap.width(), bitmap.height()) == overall_size) {
    shader_view_.Release();
    texture_.Release();

    D3D10_TEXTURE2D_DESC texture_desc;
    texture_desc.Width = bitmap.width();
    texture_desc.Height = bitmap.height();
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage = D3D10_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA texture_data;
    texture_data.pSysMem = converted_data.get();
    texture_data.SysMemPitch = texture_desc.Width * 4;
    texture_data.SysMemSlicePitch = 0;
    RETURN_IF_FAILED(device_->CreateTexture2D(&texture_desc,
                                              &texture_data,
                                              texture_.Receive()));
    RETURN_IF_FAILED(
        device_->CreateShaderResourceView(texture_.get(), NULL,
                                          shader_view_.Receive()));
  } else {
    // Only part of the texture was updated.
    DCHECK(texture_.get());
    D3D10_BOX dst_box = { origin.x(), origin.y(), 0,
                          origin.x() + bitmap.width(),
                          origin.y() + bitmap.height(), 0 };
    device_->UpdateSubresource(texture_.get(), 0, &dst_box,
                               converted_data.get(), bitmap.width() * 4, 0);
  }
}

void ViewTexture::Draw(const ui::Transform& transform) {
  host_->UpdatePerspective(transform, view_size_);

  // Make texture active.
  RETURN_IF_FAILED(
      effect_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(shader_view_.get()));

  ID3D10EffectTechnique* technique = effect_->GetTechniqueByName("ViewTech");
  DCHECK(technique);
  D3D10_TECHNIQUE_DESC tech_desc;
  technique->GetDesc(&tech_desc);
  for(UINT p = 0; p < tech_desc.Passes; ++p)
    technique->GetPassByIndex(p)->Apply(0);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D10Buffer* vertex_buffer = vertex_buffer_.get();
  device_->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
  device_->IASetIndexBuffer(index_buffer_.get(), DXGI_FORMAT_R32_UINT, 0);
  device_->DrawIndexed(6, 0, 0);
}

void ViewTexture::Errored(HRESULT result) {
  // TODO: figure out error handling.
  DCHECK(false);
}

void ViewTexture::ConvertBitmapToD3DData(const SkBitmap& bitmap,
                                         scoped_array<uint32>* converted_data) {
  int width = bitmap.width();
  int height = bitmap.height();
  SkAutoLockPixels pixel_lock(bitmap);
  // D3D wants the data in a different format (and not pre-multiplied).
  converted_data->reset(new uint32[width * height]);
  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      SkColor color = bitmap.getColor(x, y);
      int alpha = SkColorGetA(color);
      (*converted_data)[y * width + x] =
          (SkColorGetA(color) << 24) |
          (SkColorGetB(color) << 16) |
          (SkColorGetG(color) << 8) |
          (SkColorGetR(color));
    }
  }
}

void ViewTexture::CreateVertexBuffer(const gfx::Size& size) {
  vertex_buffer_.Release();
  const gfx::Size& host_size = host_->GetHostSize();
  float x = static_cast<float>(host_size.width()) / 2.0f;
  float y = static_cast<float>(host_size.height()) / 2.0f;
  float w = static_cast<float>(size.width());
  float h = static_cast<float>(size.height());
  Vertex vertices[] = {
    { D3DXVECTOR3(0.0f,   -h, 0.0f), D3DXVECTOR2(0.0f, 1.0f) },
    { D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR2(0.0f, 0.0f) },
    { D3DXVECTOR3(   w, 0.0f, 0.0f), D3DXVECTOR2(1.0f, 0.0f) },
    { D3DXVECTOR3(   w,   -h, 0.0f), D3DXVECTOR2(1.0f, 1.0f) },
  };

  // Create the vertex buffer containing the points.
  D3D10_BUFFER_DESC buffer_desc;
  buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
  buffer_desc.ByteWidth = sizeof(Vertex) * ARRAYSIZE_UNSAFE(vertices);
  buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = 0;
  buffer_desc.MiscFlags = 0;
  D3D10_SUBRESOURCE_DATA init_data;
  init_data.pSysMem = vertices;
  RETURN_IF_FAILED(device_->CreateBuffer(&buffer_desc, &init_data,
                                         vertex_buffer_.Receive()));
}

void ViewTexture::CreateIndexBuffer() {
  index_buffer_.Release();

  // Then the index buffer.
  DWORD indices[] = {
    0, 1, 2,
    0, 2, 3,
  };
  D3D10_BUFFER_DESC index_buffer;
  index_buffer.Usage = D3D10_USAGE_IMMUTABLE;
  index_buffer.ByteWidth = sizeof(DWORD) * ARRAYSIZE_UNSAFE(indices);
  index_buffer.BindFlags = D3D10_BIND_INDEX_BUFFER;
  index_buffer.CPUAccessFlags = 0;
  index_buffer.MiscFlags = 0;
  D3D10_SUBRESOURCE_DATA init_data2;
  init_data2.pSysMem = indices;
  RETURN_IF_FAILED(device_->CreateBuffer(&index_buffer, &init_data2,
                                         index_buffer_.Receive()));
}

// D3D 10 Compositor implementation.
class CompositorWin : public Compositor, public ViewTextureHost {
 public:
  explicit CompositorWin(gfx::AcceleratedWidget widget);

  void Init();

  // ViewTextureHost.
  virtual void UpdatePerspective(const ui::Transform& transform,
                                 const gfx::Size& view_size) OVERRIDE;
  virtual const gfx::Size& GetHostSize() OVERRIDE;

  // Compositor:
  virtual Texture* CreateTexture() OVERRIDE;
  virtual void NotifyStart() OVERRIDE;
  virtual void NotifyEnd() OVERRIDE;

 private:
  ~CompositorWin();

  void Errored(HRESULT error_code);

  // Returns the bounds of the hosting window.
  gfx::Rect HostBounds();

  void CreateDevice();

  void LoadEffects();

  void InitVertexLayout();

  void Resize(const gfx::Rect& bounds);

  gfx::AcceleratedWidget host_;

  // Bounds the device was last created at.
  gfx::Rect last_bounds_;

  ScopedComPtr<ID3D10Device> device_;
  ScopedComPtr<IDXGISwapChain> swap_chain_;
  ScopedComPtr<ID3D10RenderTargetView> render_target_view_;
  ScopedComPtr<ID3D10Texture2D> depth_stencil_buffer_;
  ScopedComPtr<ID3D10DepthStencilView> depth_stencil_view_;
  ScopedComPtr<ID3D10Effect, NULL> fx_;
  ID3D10EffectTechnique* technique_;
  ScopedComPtr<ID3D10InputLayout> vertex_layout_;

  DISALLOW_COPY_AND_ASSIGN(CompositorWin);
};

CompositorWin::CompositorWin(gfx::AcceleratedWidget widget)
    : host_(widget),
      technique_(NULL) {
}

void CompositorWin::Init() {
  CreateDevice();
  LoadEffects();
  Resize(last_bounds_);
  InitVertexLayout();
}

void CompositorWin::UpdatePerspective(const ui::Transform& transform,
                                      const gfx::Size& view_size) {
  // Apply transform from view.
  const SkMatrix& sk_matrix(transform.matrix());
  // Use -1 * kMTransY for y-translation as origin for views is upper left.
  D3DXMATRIX transform_matrix(
      // row 1
      sk_matrix[SkMatrix::kMScaleX], sk_matrix[SkMatrix::kMSkewX], 0.0f,
      sk_matrix[SkMatrix::kMPersp0],
      // row 2
      sk_matrix[SkMatrix::kMSkewY], sk_matrix[SkMatrix::kMScaleY], 0.0f,
      sk_matrix[SkMatrix::kMPersp1],
      // row 3
      0.0f, 0.0f, 1.0f, sk_matrix[SkMatrix::kMPersp2],
      // row 4.
      sk_matrix[SkMatrix::kMTransX], -sk_matrix[SkMatrix::kMTransY], 0.0f,
      1.0f);

  // Scale so x and y are from 0-2.
  D3DXMATRIX scale_matrix;
  D3DXMatrixScaling(
      &scale_matrix,
      2.0f / static_cast<float>(last_bounds_.width()),
      2.0f / static_cast<float>(last_bounds_.height()),
      1.0f);

  // Translate so x and y are from -1,-1 to 1,1.
  D3DXMATRIX translate_matrix;
  D3DXMatrixTranslation(&translate_matrix, -1.0f, 1.0f, 0.0f);

  D3DXMATRIX projection_matrix;
  D3DXMatrixIdentity(&projection_matrix);
  D3DXMatrixPerspectiveFovLH(&projection_matrix,
                             atanf(.5f) * 2.0f, 1.0f, 1.0f, 1000.0f);
  D3DXVECTOR3 pos(0.0f, 0.0f, -2.0f);
  D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
  D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
  D3DXMATRIX view;
  D3DXMatrixIdentity(&view);
  D3DXMatrixLookAtLH(&view, &pos, &target, &up);

  D3DXMATRIX wvp = transform_matrix * scale_matrix * translate_matrix * view *
      projection_matrix;
  fx_->GetVariableByName("gWVP")->AsMatrix()->SetMatrix((float*)&wvp);
}

const gfx::Size& CompositorWin::GetHostSize() {
  return last_bounds_.size();
}

Texture* CompositorWin::CreateTexture() {
  ViewTexture* texture = new ViewTexture(this, device_.get(), fx_.get());
  texture->Init();
  return texture;
}

void CompositorWin::NotifyStart() {
  gfx::Rect bounds = HostBounds();
  if (bounds != last_bounds_)
    Resize(bounds);

  // Clear the background and stencil view.
  device_->ClearRenderTargetView(render_target_view_.get(),
                                 D3DXCOLOR(0.0f, 0.0f, 0.0f, 0.0f));
  device_->ClearDepthStencilView(
      depth_stencil_view_.get(), D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL,
      1.0f, 0);

  // TODO: these steps may not be necessary each time through.
  device_->OMSetDepthStencilState(0, 0);
  float blend_factors[] = {0.0f, 0.0f, 0.0f, 0.0f};
  device_->OMSetBlendState(0, blend_factors, 0xffffffff);
  device_->IASetInputLayout(vertex_layout_.get());
  device_->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void CompositorWin::NotifyEnd() {
  swap_chain_->Present(0, 0);

  // We may delete the shader resource view before drawing again. Unset it so
  // that d3d doesn't generate a warning when we do that.
  fx_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(NULL);
  D3D10_TECHNIQUE_DESC tech_desc;
  technique_->GetDesc(&tech_desc);
  for(UINT i = 0; i < tech_desc.Passes; ++i)
    technique_->GetPassByIndex(i)->Apply(0);
}

CompositorWin::~CompositorWin() {
}

void CompositorWin::Errored(HRESULT error_code) {
  // TODO: figure out error handling.
  DCHECK(false);
}

gfx::Rect CompositorWin::HostBounds() {
  RECT client_rect;
  GetClientRect(host_, &client_rect);
  return gfx::Rect(client_rect);
}

void CompositorWin::CreateDevice() {
  last_bounds_ = HostBounds();

  DXGI_SWAP_CHAIN_DESC sd;
  sd.BufferDesc.Width = last_bounds_.width();
  sd.BufferDesc.Height = last_bounds_.height();
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

  // No multisampling.
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;

  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = host_;
  sd.Windowed = true;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd.Flags = 0;

  // Create the device.
  UINT createDeviceFlags = 0;
#if !defined(NDEBUG)
  createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif
  RETURN_IF_FAILED(
      D3D10CreateDeviceAndSwapChain(
          0,  //default adapter
          D3D10_DRIVER_TYPE_HARDWARE,
          0,  // no software device
          createDeviceFlags,
          D3D10_SDK_VERSION,
          &sd,
          swap_chain_.Receive(),
          device_.Receive()));
}

void CompositorWin::LoadEffects() {
  DWORD shader_flags = D3D10_SHADER_ENABLE_STRICTNESS;
#if !defined(NDEBUG)
  shader_flags |= D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;
#endif
  ScopedComPtr<ID3D10Blob> compilation_errors;
  const base::StringPiece& fx_data = ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_COMPOSITOR_FX);
  DCHECK(!fx_data.empty());
  RETURN_IF_FAILED(
      D3DX10CreateEffectFromMemory(
          fx_data.data(), fx_data.size(), "compositor.fx", NULL, NULL,
          "fx_4_0", shader_flags, 0, device_.get(), NULL, NULL, fx_.Receive(),
          compilation_errors.Receive(), NULL));
  technique_ = fx_->GetTechniqueByName("ViewTech");
  DCHECK(technique_);
}

void CompositorWin::InitVertexLayout() {
  D3D10_INPUT_ELEMENT_DESC vertex_desc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
      D3D10_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXC", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
      D3D10_INPUT_PER_VERTEX_DATA, 0 },
  };

  // Create the input layout
  D3D10_PASS_DESC pass_desc;
  RETURN_IF_FAILED(technique_->GetPassByIndex(0)->GetDesc(&pass_desc));
  RETURN_IF_FAILED(
      device_->CreateInputLayout(vertex_desc, ARRAYSIZE_UNSAFE(vertex_desc),
                                 pass_desc.pIAInputSignature,
                                 pass_desc.IAInputSignatureSize,
                                 vertex_layout_.Receive()));
}

void CompositorWin::Resize(const gfx::Rect& bounds) {
  render_target_view_ = NULL;
  depth_stencil_buffer_ = NULL;
  depth_stencil_view_ = NULL;

  // Resize the swap chain and recreate the render target view.
  RETURN_IF_FAILED(swap_chain_->ResizeBuffers(
      1, bounds.width(), bounds.height(), DXGI_FORMAT_R8G8B8A8_UNORM, 0));
  ScopedComPtr<ID3D10Texture2D> back_buffer;
  RETURN_IF_FAILED(swap_chain_->GetBuffer(
                       0, __uuidof(ID3D10Texture2D),
                       reinterpret_cast<void**>(back_buffer.Receive())));
  RETURN_IF_FAILED(device_->CreateRenderTargetView(
                       back_buffer.get(), 0, render_target_view_.Receive()));

  // Create the depth/stencil buffer and view.
  D3D10_TEXTURE2D_DESC depth_stencil_desc;
  depth_stencil_desc.Width     = bounds.width();
  depth_stencil_desc.Height    = bounds.height();
  depth_stencil_desc.MipLevels = 1;
  depth_stencil_desc.ArraySize = 1;
  depth_stencil_desc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_stencil_desc.SampleDesc.Count   = 1;  // multisampling must match
  depth_stencil_desc.SampleDesc.Quality = 0;  // swap chain values.
  depth_stencil_desc.Usage          = D3D10_USAGE_DEFAULT;
  depth_stencil_desc.BindFlags      = D3D10_BIND_DEPTH_STENCIL;
  depth_stencil_desc.CPUAccessFlags = 0;
  depth_stencil_desc.MiscFlags      = 0;

  RETURN_IF_FAILED(device_->CreateTexture2D(&depth_stencil_desc, 0,
                                            depth_stencil_buffer_.Receive()));
  RETURN_IF_FAILED(device_->CreateDepthStencilView(
                       depth_stencil_buffer_.get(), 0,
                       depth_stencil_view_.Receive()));


  // Bind the render target view and depth/stencil view to the pipeline.
  ID3D10RenderTargetView* target_view = render_target_view_.get();
  device_->OMSetRenderTargets(1, &target_view, depth_stencil_view_.get());

  // Set the viewport transform.
  D3D10_VIEWPORT vp;
  vp.TopLeftX = bounds.x();
  vp.TopLeftY = bounds.y();
  vp.Width    = bounds.width();
  vp.Height   = bounds.height();
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;

  device_->RSSetViewports(1, &vp);

  last_bounds_ = bounds;
}

}  // namespace

// static
Compositor* Compositor::Create(gfx::AcceleratedWidget widget) {
  CompositorWin* compositor = new CompositorWin(widget);
  compositor->Init();
  return compositor;
}

}  // namespace ui
