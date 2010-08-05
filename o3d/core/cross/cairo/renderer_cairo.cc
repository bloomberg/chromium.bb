/*
 * Copyright 2010, Google Inc.
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

// Renderer that is using 2D Library Cairo.

#include "core/cross/cairo/renderer_cairo.h"
#include <cairo-xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/cross/cairo/texture_cairo.h"

namespace o3d {

Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererCairo::CreateDefault(service_locator);
}

RendererCairo::RendererCairo(ServiceLocator* service_locator)
    : Renderer(service_locator), display_(NULL), window_(0),
      main_surface_(NULL), frame_src_data_(NULL), frame_src_width_(0),
      frame_src_height_(0), frame_src_pitch_(0) {
  // Don't need to do anything.
}

RendererCairo::~RendererCairo() {
  Destroy();
}

RendererCairo* RendererCairo::CreateDefault(ServiceLocator* service_locator) {
  return new RendererCairo(service_locator);
}

// Released all hardware resources.
void RendererCairo::Destroy() {
  DLOG(INFO) << "To Destroy";

  if (main_surface_ != NULL) {
    cairo_surface_destroy(main_surface_);
    main_surface_= NULL;
  }

  display_ = NULL;
  frame_src_data_ = NULL;
}

void RendererCairo::SetNewFrame(const void* src_data, unsigned src_width,
                                unsigned src_height, int src_pitch) {
  DLOG(INFO) << "To Set New Frame";
  if (src_data == NULL)
    return;

  frame_src_data_ = src_data;
  frame_src_width_ = src_width;
  frame_src_height_ = src_height;
  frame_src_pitch_ = src_pitch;
}

// TODO(fransiskusx): Need to check if the shared memory data has been
// unregistered. It will prevent errors when accessing unregistered
// shared memory data to render the frame.
void RendererCairo::Paint() {
  DLOG(INFO) << "To paint";

  if (frame_src_data_ != NULL) {
    DLOG(INFO) << "To paint new drawing";

    // Preparing the image to render
    cairo_surface_t* image = cairo_image_surface_create_for_data(
        const_cast<unsigned char*>(
            static_cast<const unsigned char*>(frame_src_data_)),
        CAIRO_FORMAT_ARGB32, frame_src_width_,
        frame_src_height_, frame_src_pitch_);
    cairo_t* current_drawing = cairo_create(main_surface_);

    // Scaling the image
    double width_scaling =
        (static_cast<double>(display_width())) / frame_src_width_;
    double height_scaling =
        (static_cast<double>(display_height())) / frame_src_height_;
    cairo_scale(current_drawing, width_scaling, height_scaling);

    // Painting the image to the surface
    cairo_set_source_surface(current_drawing, image, 0, 0);
    cairo_paint(current_drawing);

    // Cleaning up the memory
    cairo_destroy(current_drawing);
    cairo_surface_destroy(image);
  }
}

void RendererCairo::InitCommon() {
  main_surface_ = cairo_xlib_surface_create(display_, window_,
                                            XDefaultVisual(display_, 0),
                                            display_width(), display_height());
}

void RendererCairo::UninitCommon() {
  // Don't need to do anything.
}

Renderer::InitStatus RendererCairo::InitPlatformSpecific(
    const DisplayWindow& display_window,
    bool off_screen) {
  const DisplayWindowLinux &display_platform =
      static_cast<const DisplayWindowLinux&>(display_window);
  display_ = display_platform.display();
  window_ = display_platform.window();

  return SUCCESS;
}

// Handles the plugin resize event.
void RendererCairo::Resize(int width, int height) {
  DLOG(INFO) << "To Resize " << width << " x " << height;
  SetClientSize(width, height);

  // Resize the mainSurface
  cairo_xlib_surface_set_size(main_surface_, width, height);
}

// The platform specific part of BeginDraw.
bool RendererCairo::PlatformSpecificBeginDraw() {
  return true;
}

// Platform specific version of CreateTexture2D
Texture2D::Ref RendererCairo::CreatePlatformSpecificTexture2D(
    int width,
    int height,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  return Texture2D::Ref(TextureCairo::Create(service_locator(),
                                             format,
                                             levels,
                                             width,
                                             height,
                                             enable_render_surfaces));
}

// The platform specific part of EndDraw.
void RendererCairo::PlatformSpecificEndDraw() {
  // Don't need to do anything.
}

// The platform specific part of StartRendering.
bool RendererCairo::PlatformSpecificStartRendering() {
  return true;
}

// The platform specific part of EndRendering.
void RendererCairo::PlatformSpecificFinishRendering() {
  Paint();
}

// The platform specific part of Present.
void RendererCairo::PlatformSpecificPresent() {
  // Don't need to do anything.
}

// TODO(fransiskusx): DO need to implement before shipped.
// Get a single fullscreen display mode by id.
// Returns true on success, false on error.
bool RendererCairo::GetDisplayMode(int id, DisplayMode* mode) {
  NOTIMPLEMENTED();
  return true;
}

// TODO(fransiskusx):  DO need to implement before shipped.
// Get a vector of the available fullscreen display modes.
// Clears *modes on error.
void RendererCairo::GetDisplayModes(std::vector<DisplayMode>* modes) {
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): DO need to implement before shipped.
// The platform specific part of Clear.
void RendererCairo::PlatformSpecificClear(const Float4 &color,
                                          bool color_flag,
                                          float depth,
                                          bool depth_flag,
                                          int stencil,
                                          bool stencil_flag) {
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): DO need to implement before shipped.
// Sets the viewport. This is the platform specific version.
void RendererCairo::SetViewportInPixels(int left,
                                        int top,
                                        int width,
                                        int height,
                                        float min_z,
                                        float max_z) {
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): Need to implement it later.
// Turns fullscreen display on.
// Parameters:
//  display: a platform-specific display identifier
//  mode_id: a mode returned by GetDisplayModes
// Returns true on success, false on failure.
bool RendererCairo::GoFullscreen(const DisplayWindow& display,
                                 int mode_id) {
  NOTIMPLEMENTED();
  return false;
}

// TODO(fransiskusx): Need to implement it later.
// Cancels fullscreen display. Restores rendering to windowed mode
// with the given width and height.
// Parameters:
//  display: a platform-specific display identifier
//  width: the width to which to restore windowed rendering
//  height: the height to which to restore windowed rendering
// Returns true on success, false on failure.
bool RendererCairo::CancelFullscreen(const DisplayWindow& display,
                                     int width, int height) {
  NOTIMPLEMENTED();
  return false;
}

// TODO(fransiskusx): Need to implement it later.
// Tells whether we're currently displayed fullscreen or not.
bool RendererCairo::fullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Sets rendering to the back buffer.
void RendererCairo::SetBackBufferPlatformSpecific() {
  // Don't need to do anything.
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Applies states that have been modified (marked dirty).
void RendererCairo::ApplyDirtyStates() {
  // Don't need to do anything.
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates a platform specific ParamCache.
ParamCache* RendererCairo::CreatePlatformSpecificParamCache() {
  NOTIMPLEMENTED();
  return NULL;
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Platform specific version of CreateTextureCUBE.
TextureCUBE::Ref RendererCairo::CreatePlatformSpecificTextureCUBE(
    int edge_length,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  NOTIMPLEMENTED();
  return TextureCUBE::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
void RendererCairo::PushRenderStates(State* state) {
  // Don't need to do anything.;
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Sets the render surfaces on a specific platform.
void RendererCairo::SetRenderSurfacesPlatformSpecific(
    const RenderSurface* surface,
    const RenderDepthStencilSurface* depth_surface) {
  // Don't need to do anything.
  NOTIMPLEMENTED();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates a StreamBank, returning a platform specific implementation class.
StreamBank::Ref RendererCairo::CreateStreamBank() {
  NOTIMPLEMENTED();
  return StreamBank::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates a Primitive, returning a platform specific implementation class.
Primitive::Ref RendererCairo::CreatePrimitive() {
  NOTIMPLEMENTED();
  return Primitive::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates a DrawElement, returning a platform specific implementation
// class.
DrawElement::Ref RendererCairo::CreateDrawElement() {
  NOTIMPLEMENTED();
  return DrawElement::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates and returns a platform specific float buffer
VertexBuffer::Ref RendererCairo::CreateVertexBuffer() {
  NOTIMPLEMENTED();
  return VertexBuffer::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates and returns a platform specific integer buffer
IndexBuffer::Ref RendererCairo::CreateIndexBuffer() {
  NOTIMPLEMENTED();
  return IndexBuffer::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates and returns a platform specific effect object
Effect::Ref RendererCairo::CreateEffect() {
  NOTIMPLEMENTED();
  return Effect::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates and returns a platform specific Sampler object.
Sampler::Ref RendererCairo::CreateSampler() {
  NOTIMPLEMENTED();
  return Sampler::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Returns a platform specific 4 element swizzle table for RGBA UByteN fields.
// The should contain the index of R, G, B, and A in that order for the
// current platform.
const int* RendererCairo::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 0, 1, 2, 3, };
  NOTIMPLEMENTED();
  return swizzle_table;
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
// Creates and returns a platform-specific RenderDepthStencilSurface object
// for use as a depth-stencil render target.
RenderDepthStencilSurface::Ref RendererCairo::CreateDepthStencilSurface(
    int width,
    int height) {
  NOTIMPLEMENTED();
  return RenderDepthStencilSurface::Ref();
}

// TODO(fransiskusx): This function is not applicable to 2D rendering.
void RendererCairo::SetState(Renderer* renderer, Param* param) {
  // Don't need to do anything.
  NOTIMPLEMENTED();
}

}  // namespace o3d

