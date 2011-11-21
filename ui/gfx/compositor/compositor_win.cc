// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include <algorithm>
#include <d3dx10.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/win/scoped_comptr.h"
#include "grit/gfx_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
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

class CompositorWin;

// Vertex structure used by the compositor.
struct Vertex {
  D3DXVECTOR3 position;
  D3DXVECTOR2 texture_offset;
};

// D3D 10 Texture implementation. Creates a quad representing the view and
// a texture with the bitmap data. The quad has an origin of 0,0,0 with a size
// matching that of |SetCanvas|.
class ViewTexture : public Texture {
 public:
  ViewTexture(CompositorWin* compositor,
              ID3D10Device* device,
              ID3D10Effect* effect);

  // Texture:
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 private:
  ~ViewTexture();

  void Errored(HRESULT result);

  void ConvertBitmapToD3DData(const SkBitmap& bitmap,
                              scoped_array<uint32>* converted_data);

  // Creates vertex buffer for specified region
  void CreateVertexBufferForRegion(const gfx::Rect& bounds);

  scoped_refptr<CompositorWin> compositor_;

  // Size of the corresponding View.
  gfx::Size view_size_;

  ScopedComPtr<ID3D10Device> device_;
  ScopedComPtr<ID3D10Effect, NULL> effect_;
  ScopedComPtr<ID3D10Texture2D> texture_;
  ScopedComPtr<ID3D10ShaderResourceView> shader_view_;
  ScopedComPtr<ID3D10Buffer> vertex_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ViewTexture);
};

// D3D 10 Compositor implementation.
class CompositorWin : public Compositor {
 public:
  CompositorWin(CompositorDelegate* delegate,
                gfx::AcceleratedWidget widget,
                const gfx::Size& size);

  void Init();

  // Invoked to update the perspective needed by this texture. |transform| is
  // the transform for the texture, and |size| the size of the texture.
  void UpdatePerspective(const ui::Transform& transform,
                         const gfx::Size& view_size);

  // Returns the index buffer used for drawing a texture.
  ID3D10Buffer* GetTextureIndexBuffer();

  // Compositor:
  virtual Texture* CreateTexture() OVERRIDE;

  virtual void Blur(const gfx::Rect& bounds) OVERRIDE;

  virtual bool ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) OVERRIDE;

 protected:
  virtual void OnNotifyStart(bool clear) OVERRIDE;
  virtual void OnNotifyEnd() OVERRIDE;
  virtual void OnWidgetSizeChanged() OVERRIDE;

 private:
  enum Direction {
    HORIZONTAL,
    VERTICAL
  };

  ~CompositorWin();

  void Errored(HRESULT error_code);

  void CreateDevice();

  void LoadEffects();

  void InitVertexLayout();

  // Updates the kernel used for blurring. Size is the size of the texture
  // being drawn to along the appropriate axis.
  void UpdateBlurKernel(Direction direction, int size);

  // Creates a texture, render target view and shader.
  void CreateTexture(const gfx::Size& size,
                     ID3D10Texture2D** texture,
                     ID3D10RenderTargetView** render_target_view,
                     ID3D10ShaderResourceView** shader_resource_view);

  // Creates |vertex_buffer_|.
  void CreateVertexBuffer();

  // Creates |index_buffer_|.
  void CreateIndexBuffer();

  // Creates a vertex buffer for the specified region. The caller owns the
  // return value.
  ID3D10Buffer* CreateVertexBufferForRegion(const gfx::Rect& bounds);

  gfx::AcceleratedWidget host_;

  ScopedComPtr<ID3D10Device> device_;
  ScopedComPtr<IDXGISwapChain> swap_chain_;
  ScopedComPtr<ID3D10RenderTargetView> dest_render_target_view_;
  ScopedComPtr<ID3D10Texture2D> depth_stencil_buffer_;
  ScopedComPtr<ID3D10DepthStencilView> depth_stencil_view_;
  ScopedComPtr<ID3D10Effect, NULL> fx_;
  ID3D10EffectTechnique* technique_;

  // Layout for Vertex.
  ScopedComPtr<ID3D10InputLayout> vertex_layout_;

  // Identity vertext buffer. Used when copying from main back to dest.
  ScopedComPtr<ID3D10Buffer> vertex_buffer_;

  // Index buffer used for drawing a rectangle.
  ScopedComPtr<ID3D10Buffer> index_buffer_;

  // Used for bluring.
  ScopedComPtr<ID3D10Effect, NULL> blur_fx_;
  ID3D10EffectTechnique* blur_technique_;

  // All rendering is done to the main_texture. Effects (such as bloom) render
  // into the blur texture, and are then copied back to the main texture. When
  // rendering is done |main_texture_| is drawn back to
  // |dest_render_target_view_|.
  ScopedComPtr<ID3D10Texture2D> main_texture_;
  ScopedComPtr<ID3D10RenderTargetView> main_render_target_view_;
  ScopedComPtr<ID3D10ShaderResourceView> main_texture_shader_view_;

  ScopedComPtr<ID3D10Texture2D> blur_texture_;
  ScopedComPtr<ID3D10RenderTargetView> blur_render_target_view_;
  ScopedComPtr<ID3D10ShaderResourceView> blur_texture_shader_view_;

  DISALLOW_COPY_AND_ASSIGN(CompositorWin);
};

ViewTexture::ViewTexture(CompositorWin* compositor,
                         ID3D10Device* device,
                         ID3D10Effect* effect)
    : compositor_(compositor),
      device_(device),
      effect_(effect) {
}

ViewTexture::~ViewTexture() {
}

void ViewTexture::SetCanvas(const SkCanvas& canvas,
                            const gfx::Point& origin,
                            const gfx::Size& overall_size) {
  view_size_ = overall_size;

  scoped_array<uint32> converted_data;
  const SkBitmap& bitmap = canvas.getDevice()->accessBitmap(false);
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
    RETURN_IF_FAILED(device_->CreateTexture2D(&texture_desc,
                                              NULL, texture_.Receive()));
    RETURN_IF_FAILED(
        device_->CreateShaderResourceView(texture_.get(), NULL,
                                          shader_view_.Receive()));
  }
  DCHECK(texture_.get());
  D3D10_BOX dst_box = { origin.x(), origin.y(), 0,
                        origin.x() + bitmap.width(),
                        origin.y() + bitmap.height(), 1 };
  device_->UpdateSubresource(texture_.get(), 0, &dst_box,
                             converted_data.get(), bitmap.width() * 4, 0);
}

void ViewTexture::Draw(const ui::TextureDrawParams& params,
                       const gfx::Rect& clip_bounds) {
  if (params.vertically_flipped)
    NOTIMPLEMENTED();

  compositor_->UpdatePerspective(params.transform, view_size_);

  // Make texture active.
  RETURN_IF_FAILED(
      effect_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(shader_view_.get()));

  RETURN_IF_FAILED(effect_->GetVariableByName("alpha")->AsScalar()->SetFloat(
                   params.opacity));

  ID3D10EffectTechnique* technique = effect_->GetTechniqueByName("ViewTech");
  DCHECK(technique);
  D3D10_TECHNIQUE_DESC tech_desc;
  technique->GetDesc(&tech_desc);
  for(UINT p = 0; p < tech_desc.Passes; ++p)
    technique->GetPassByIndex(p)->Apply(0);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  CreateVertexBufferForRegion(clip_bounds);
  ID3D10Buffer* vertex_buffer = vertex_buffer_.get();
  device_->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
  device_->IASetIndexBuffer(compositor_->GetTextureIndexBuffer(),
                            DXGI_FORMAT_R32_UINT, 0);
  device_->DrawIndexed(6, 0, 0);
}

void ViewTexture::Errored(HRESULT result) {
  // TODO: figure out error handling.
  DCHECK(false);
}

void ViewTexture::ConvertBitmapToD3DData(
    const SkBitmap& bitmap,
    scoped_array<uint32>* converted_data) {
  int width = bitmap.width();
  int height = bitmap.height();
  SkAutoLockPixels pixel_lock(bitmap);
  // D3D wants colors in ABGR format (premultiplied).
  converted_data->reset(new uint32[width * height]);
  uint32_t* bitmap_data = bitmap.getAddr32(0, 0);
  for (int x = 0, offset = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y, ++offset) {
      SkColor color = bitmap_data[offset];
      (*converted_data)[offset] =
          (SkColorGetA(color) << 24) |
          (SkColorGetB(color) << 16) |
          (SkColorGetG(color) << 8) |
          (SkColorGetR(color));
    }
  }
}

void ViewTexture::CreateVertexBufferForRegion(const gfx::Rect& bounds) {
  vertex_buffer_.Release();
  float x = bounds.x();
  float max_x = bounds.right();
  float y = bounds.y();
  float max_y = bounds.bottom();
  float tex_x = x / static_cast<float>(view_size_.width());
  float max_tex_x = max_x / static_cast<float>(view_size_.width());
  float tex_y = y / static_cast<float>(view_size_.width());
  float max_tex_y = max_y / static_cast<float>(view_size_.height());
  Vertex vertices[] = {
    { D3DXVECTOR3(    x, -max_y, 0.0f), D3DXVECTOR2(    tex_x, max_tex_y) },
    { D3DXVECTOR3(    x,     -y, 0.0f), D3DXVECTOR2(    tex_x,     tex_y) },
    { D3DXVECTOR3(max_x,     -y, 0.0f), D3DXVECTOR2(max_tex_x,     tex_y) },
    { D3DXVECTOR3(max_x, -max_y, 0.0f), D3DXVECTOR2(max_tex_x, max_tex_y) }
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

CompositorWin::CompositorWin(CompositorDelegate* delegate,
                             gfx::AcceleratedWidget widget,
                             const gfx::Size& size)
    : Compositor(delegate, size),
      host_(widget),
      technique_(NULL) {
}

void CompositorWin::Init() {
  CreateDevice();
  LoadEffects();
  OnWidgetSizeChanged();
  InitVertexLayout();
  CreateVertexBuffer();
  CreateIndexBuffer();
}

void CompositorWin::UpdatePerspective(const ui::Transform& transform,
                                      const gfx::Size& view_size) {
  float transform_data_buffer[16];
  transform.matrix().asColMajorf(transform_data_buffer);
  D3DXMATRIX transform_matrix(&transform_data_buffer[0]);
  std::swap(transform_matrix._12, transform_matrix._21);
  std::swap(transform_matrix._13, transform_matrix._31);
  std::swap(transform_matrix._23, transform_matrix._32);

  // Different coordinate system; flip the y.
  transform_matrix._42 *= -1;

  // Scale so x and y are from 0-2.
  D3DXMATRIX scale_matrix;
  D3DXMatrixScaling(
      &scale_matrix,
      2.0f / static_cast<float>(size().width()),
      2.0f / static_cast<float>(size().height()),
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

  fx_->GetVariableByName("gWVP")->AsMatrix()->SetMatrix(wvp);
}

ID3D10Buffer* CompositorWin::GetTextureIndexBuffer() {
  return index_buffer_.get();
}

Texture* CompositorWin::CreateTexture() {
  return new ViewTexture(this, device_.get(), fx_.get());
}

void CompositorWin::OnNotifyStart(bool clear) {
  ID3D10RenderTargetView* target_view = main_render_target_view_.get();
  device_->OMSetRenderTargets(1, &target_view, depth_stencil_view_.get());

  // Clear the background and stencil view.
  // TODO(vollick) see if |clear| can be used to avoid unnecessary clearing.
  device_->ClearRenderTargetView(target_view,
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

void CompositorWin::OnNotifyEnd() {
  // Copy from main_render_target_view_| (where all are rendering was done) back
  // to |dest_render_target_view_|.
  ID3D10RenderTargetView* target_view = dest_render_target_view_.get();
  device_->OMSetRenderTargets(1, &target_view, NULL);
  device_->ClearRenderTargetView(target_view,
                                 D3DXCOLOR(0.0f, 0.0f, 0.0f, 0.0f));
  RETURN_IF_FAILED(
      fx_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(main_texture_shader_view_.get()));
  D3DXMATRIX identify_matrix;
  D3DXMatrixIdentity(&identify_matrix);
  fx_->GetVariableByName("gWVP")->AsMatrix()->SetMatrix(identify_matrix);
  ID3D10EffectTechnique* technique = fx_->GetTechniqueByName("ViewTech");
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
  RETURN_IF_FAILED(
      fx_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(NULL));
  swap_chain_->Present(0, 0);

  // We may delete the shader resource view before drawing again. Unset it so
  // that d3d doesn't generate a warning when we do that.
  fx_->GetVariableByName("textureMap")->AsShaderResource()->SetResource(NULL);
  for(UINT i = 0; i < tech_desc.Passes; ++i)
    technique_->GetPassByIndex(i)->Apply(0);
}

void CompositorWin::Blur(const gfx::Rect& bounds) {
  // Set up the vertex buffer for the region we're going to blur.
  ScopedComPtr<ID3D10Buffer> scoped_vertex_buffer(
      CreateVertexBufferForRegion(bounds));
  ID3D10Buffer* vertex_buffer = scoped_vertex_buffer.get();
  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  device_->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
  device_->IASetIndexBuffer(index_buffer_.get(), DXGI_FORMAT_R32_UINT, 0);
  device_->DrawIndexed(6, 0, 0);
  D3DXMATRIX identity_matrix;
  D3DXMatrixIdentity(&identity_matrix);

  // Horizontal blur from the main texture to blur texture.
  UpdateBlurKernel(HORIZONTAL, size().width());
  ID3D10RenderTargetView* target_view = blur_render_target_view_.get();
  device_->OMSetRenderTargets(1, &target_view, NULL);
  RETURN_IF_FAILED(
      blur_fx_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(main_texture_shader_view_.get()));
  blur_fx_->GetVariableByName("gWVP")->AsMatrix()->SetMatrix(
      identity_matrix);
  ID3D10EffectTechnique* technique = blur_fx_->GetTechniqueByName("ViewTech");
  DCHECK(technique);
  D3D10_TECHNIQUE_DESC tech_desc;
  technique->GetDesc(&tech_desc);
  for(UINT p = 0; p < tech_desc.Passes; ++p)
    technique->GetPassByIndex(p)->Apply(0);
  device_->DrawIndexed(6, 0, 0);

  {
    // We do this to avoid a warning.
    ID3D10ShaderResourceView* tmp = NULL;
    device_->PSSetShaderResources(0, 1, &tmp);
  }

  // Vertical blur from the blur texture back to main buffer.
  RETURN_IF_FAILED(
      blur_fx_->GetVariableByName("textureMap")->AsShaderResource()->
      SetResource(blur_texture_shader_view_.get()));
  UpdateBlurKernel(VERTICAL, size().height());
  target_view = main_render_target_view_.get();
  device_->OMSetRenderTargets(1, &target_view, NULL);
  for(UINT p = 0; p < tech_desc.Passes; ++p)
    technique->GetPassByIndex(p)->Apply(0);
  device_->DrawIndexed(6, 0, 0);

#if !defined(NDEBUG)
  // A warning is generated if a bound vertexbuffer is deleted. To avoid that
  // warning we reset the buffer here. We only do it for debug builds as it's
  // ok to effectively unbind the vertex buffer.
  vertex_buffer = vertex_buffer_.get();
  device_->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
  device_->IASetIndexBuffer(index_buffer_.get(), DXGI_FORMAT_R32_UINT, 0);
#endif
}

bool CompositorWin::ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
  return false;
}

void CompositorWin::OnWidgetSizeChanged() {
  dest_render_target_view_ = NULL;
  depth_stencil_buffer_ = NULL;
  depth_stencil_view_ = NULL;

  main_render_target_view_ = NULL;
  main_texture_ = NULL;
  main_texture_shader_view_ = NULL;

  blur_render_target_view_ = NULL;
  blur_texture_ = NULL;
  blur_texture_shader_view_ = NULL;

  CreateTexture(size(), main_texture_.Receive(),
                main_render_target_view_.Receive(),
                main_texture_shader_view_.Receive());

  CreateTexture(size(), blur_texture_.Receive(),
                blur_render_target_view_.Receive(),
                blur_texture_shader_view_.Receive());

  // Resize the swap chain and recreate the render target view.
  RETURN_IF_FAILED(swap_chain_->ResizeBuffers(
      1, size().width(), size().height(), DXGI_FORMAT_R8G8B8A8_UNORM, 0));
  ScopedComPtr<ID3D10Texture2D> back_buffer;
  RETURN_IF_FAILED(swap_chain_->GetBuffer(
                       0, __uuidof(ID3D10Texture2D),
                       reinterpret_cast<void**>(back_buffer.Receive())));
  RETURN_IF_FAILED(device_->CreateRenderTargetView(
                       back_buffer.get(), 0,
                       dest_render_target_view_.Receive()));

  // Create the depth/stencil buffer and view.
  D3D10_TEXTURE2D_DESC depth_stencil_desc;
  depth_stencil_desc.Width     = size().width();
  depth_stencil_desc.Height    = size().height();
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


  // Set the viewport transform.
  D3D10_VIEWPORT vp;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  vp.Width    = size().width();
  vp.Height   = size().height();
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;

  device_->RSSetViewports(1, &vp);
}

CompositorWin::~CompositorWin() {
}

void CompositorWin::Errored(HRESULT error_code) {
  // TODO: figure out error handling.
  DCHECK(false);
}

void CompositorWin::CreateDevice() {
  DXGI_SWAP_CHAIN_DESC sd;
  sd.BufferDesc.Width = size().width();
  sd.BufferDesc.Height = size().height();
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

  const base::StringPiece& blur_data = ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_BLUR_FX);
  DCHECK(!blur_data.empty());
  compilation_errors = NULL;
  RETURN_IF_FAILED(
      D3DX10CreateEffectFromMemory(
          blur_data.data(), blur_data.size(), "bloom.fx", NULL, NULL,
          "fx_4_0", shader_flags, 0, device_.get(), NULL, NULL,
          blur_fx_.Receive(), compilation_errors.Receive(), NULL));
  blur_technique_ = blur_fx_->GetTechniqueByName("ViewTech");
  DCHECK(blur_technique_);
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

void CompositorWin::UpdateBlurKernel(Direction direction, int size) {
  // Update the blur data.
  const int kSize = 13;
  float pixel_data[4 * kSize];
  memset(pixel_data, 0, sizeof(float) * 4 * kSize);
  for (int i = 0; i < kSize; ++i) {
    pixel_data[i * 4 + ((direction == HORIZONTAL) ? 0 : 1)] =
        static_cast<float>(i - (kSize / 2)) / static_cast<float>(size);
  }
  RETURN_IF_FAILED(blur_fx_->GetVariableByName("TexelKernel")->AsVector()->
                   SetFloatVectorArray(pixel_data, 0, kSize));
}

void CompositorWin::CreateTexture(
    const gfx::Size& size,
    ID3D10Texture2D** texture,
    ID3D10RenderTargetView** render_target_view,
    ID3D10ShaderResourceView** shader_resource_view) {
  D3D10_TEXTURE2D_DESC texture_desc;
  texture_desc.Width = size.width();
  texture_desc.Height = size.height();
  texture_desc.MipLevels = 1;
  texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D10_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.MiscFlags = 0;
  texture_desc.ArraySize = 1;
  RETURN_IF_FAILED(device_->CreateTexture2D(&texture_desc, NULL, texture));

  RETURN_IF_FAILED(
      device_->CreateShaderResourceView(*texture, NULL, shader_resource_view));

  D3D10_RENDER_TARGET_VIEW_DESC render_target_view_desc;
  render_target_view_desc.Format = texture_desc.Format;
  render_target_view_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
  render_target_view_desc.Texture2D.MipSlice = 0;
  RETURN_IF_FAILED(device_->CreateRenderTargetView(
                       *texture, &render_target_view_desc, render_target_view));
}

void CompositorWin::CreateVertexBuffer() {
  vertex_buffer_.Release();

  Vertex vertices[] = {
    { D3DXVECTOR3(-1.0f, -1.0f, 0.0f), D3DXVECTOR2(0.0f, 1.0f) },
    { D3DXVECTOR3(-1.0f,  1.0f, 0.0f), D3DXVECTOR2(0.0f, 0.0f) },
    { D3DXVECTOR3( 1.0f,  1.0f, 0.0f), D3DXVECTOR2(1.0f, 0.0f) },
    { D3DXVECTOR3( 1.0f, -1.0f, 0.0f), D3DXVECTOR2(1.0f, 1.0f) },
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

void CompositorWin::CreateIndexBuffer() {
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
  D3D10_SUBRESOURCE_DATA init_data;
  init_data.pSysMem = indices;
  RETURN_IF_FAILED(device_->CreateBuffer(&index_buffer, &init_data,
                                         index_buffer_.Receive()));
}

ID3D10Buffer* CompositorWin::CreateVertexBufferForRegion(
    const gfx::Rect& bounds) {
  float x = static_cast<float>(bounds.x()) /
      static_cast<float>(size().width()) * 2.0f - 1.0f;
  float max_x =
      x + bounds.width() / static_cast<float>(size().width()) * 2.0f;
  float y =
      static_cast<float>(size().height() - bounds.y() - bounds.height()) /
      static_cast<float>(size().height()) * 2.0f - 1.0f;
  float max_y =
      y + bounds.height() / static_cast<float>(size().height()) * 2.0f;
  float tex_x = x / 2.0f + .5f;
  float max_tex_x = max_x / 2.0f + .5f;
  float tex_y = 1.0f - (max_y + 1.0f) / 2.0f;
  float max_tex_y = tex_y + (max_y - y) / 2.0f;
  Vertex vertices[] = {
    { D3DXVECTOR3(    x,     y, 0.0f), D3DXVECTOR2(    tex_x, max_tex_y) },
    { D3DXVECTOR3(    x, max_y, 0.0f), D3DXVECTOR2(    tex_x,     tex_y) },
    { D3DXVECTOR3(max_x, max_y, 0.0f), D3DXVECTOR2(max_tex_x,     tex_y) },
    { D3DXVECTOR3(max_x,     y, 0.0f), D3DXVECTOR2(max_tex_x, max_tex_y) },
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
  ID3D10Buffer* vertex_buffer = NULL;
  // TODO: better error handling.
  int error_code =
      device_->CreateBuffer(&buffer_desc, &init_data, &vertex_buffer);
  if (error_code != S_OK) {
    Errored(error_code);
    return NULL;
  }
  return vertex_buffer;
}

}  // namespace

// static
Compositor* Compositor::Create(CompositorDelegate* delegate,
                               gfx::AcceleratedWidget widget,
                               const gfx::Size& size) {
  CompositorWin* compositor = new CompositorWin(delegate, widget, size);
  compositor->Init();
  return compositor;
}

}  // namespace ui
