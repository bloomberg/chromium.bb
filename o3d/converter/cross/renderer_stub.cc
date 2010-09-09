/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the definition of abstract functions in the
// cross platform API so that we can use it for serialization of the
// scene graph on all systems without needing graphics.

#include "converter/cross/renderer_stub.h"
#include "core/cross/object_manager.h"
#include "core/cross/service_locator.h"
#include "converter/cross/buffer_stub.h"
#include "converter/cross/draw_element_stub.h"
#include "converter/cross/effect_stub.h"
#include "converter/cross/param_cache_stub.h"
#include "converter/cross/primitive_stub.h"
#include "converter/cross/render_surface_stub.h"
#include "converter/cross/sampler_stub.h"
#include "converter/cross/stream_bank_stub.h"
#include "converter/cross/texture_stub.h"

namespace o3d {

Renderer* RendererStub::CreateDefault(ServiceLocator* service_locator) {
  return new RendererStub(service_locator);
}

RendererStub::RendererStub(ServiceLocator* service_locator)
    : Renderer(service_locator) {
}

Renderer::InitStatus RendererStub::InitPlatformSpecific(
    const DisplayWindow&, bool) {
  DCHECK(false);
  return SUCCESS;
}

void RendererStub::InitCommon() {
}

void RendererStub::UninitCommon() {
}

void RendererStub::Destroy(void) {
  DCHECK(false);
}

bool RendererStub::PlatformSpecificBeginDraw(void) {
  DCHECK(false);
  return true;
}

void RendererStub::PlatformSpecificEndDraw(void) {
  DCHECK(false);
}

bool RendererStub::PlatformSpecificStartRendering(void) {
  DCHECK(false);
  return true;
}

void RendererStub::PlatformSpecificFinishRendering(void) {
  DCHECK(false);
}

void RendererStub::Resize(int, int) {
  DCHECK(false);
}

void RendererStub::PlatformSpecificClear(
  const Float4 &, bool, float, bool, int, bool) {
  DCHECK(false);
}

void RendererStub::SetRenderSurfacesPlatformSpecific(
    const RenderSurface* surface,
    const RenderDepthStencilSurface* surface_depth) {
  DCHECK(false);
}

void RendererStub::SetBackBufferPlatformSpecific() {
  DCHECK(false);
}

void RendererStub::ApplyDirtyStates() {
  DCHECK(false);
}

Primitive::Ref RendererStub::CreatePrimitive(void) {
  return Primitive::Ref(new PrimitiveStub(service_locator()));
}

DrawElement::Ref RendererStub::CreateDrawElement(void) {
  return DrawElement::Ref(new DrawElementStub(service_locator()));
}

VertexBuffer::Ref RendererStub::CreateVertexBuffer(void) {
  return VertexBuffer::Ref(new VertexBufferStub(service_locator()));
}

IndexBuffer::Ref RendererStub::CreateIndexBuffer(void) {
  return IndexBuffer::Ref(new IndexBufferStub(service_locator()));
}

Effect::Ref RendererStub::CreateEffect(void) {
  return Effect::Ref(new EffectStub(service_locator()));
}

Sampler::Ref RendererStub::CreateSampler(void) {
  return Sampler::Ref(new SamplerStub(service_locator()));
}

Texture2D::Ref RendererStub::CreatePlatformSpecificTexture2D(
    int width,
    int height,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  return Texture2D::Ref(new Texture2DStub(service_locator(),
                                          width,
                                          height,
                                          format,
                                          levels,
                                          enable_render_surfaces));
}

TextureCUBE::Ref RendererStub::CreatePlatformSpecificTextureCUBE(
    int edge_length,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  return TextureCUBE::Ref(new TextureCUBEStub(service_locator(),
                                              edge_length,
                                              format,
                                              levels,
                                              enable_render_surfaces));
}

RenderDepthStencilSurface::Ref RendererStub::CreateDepthStencilSurface(
    int width,
    int height) {
  return RenderDepthStencilSurface::Ref(
      new RenderDepthStencilSurfaceStub(service_locator(), width, height));
}

StreamBank::Ref RendererStub::CreateStreamBank() {
  return StreamBank::Ref(new StreamBankStub(service_locator()));
}

ParamCache *RendererStub::CreatePlatformSpecificParamCache(void) {
  return new ParamCacheStub;
}

void RendererStub::SetViewportInPixels(int, int, int, int, float, float) {
  DCHECK(false);
}

bool RendererStub::GoFullscreen(const DisplayWindow& display,
                                int mode_id) {
  return false;
}

bool RendererStub::CancelFullscreen(const DisplayWindow& display,
                                    int width, int height) {
  return false;
}

bool RendererStub::fullscreen() const { return false; }

void RendererStub::GetDisplayModes(std::vector<DisplayMode> *modes) {
  modes->clear();
}

bool RendererStub::GetDisplayMode(int id, DisplayMode *mode) {
  return false;
}

void RendererStub::PlatformSpecificPresent(void) {
}

const int* RendererStub::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 0, 1, 2, 3, };
  return swizzle_table;
}

}  // namespace o3d
