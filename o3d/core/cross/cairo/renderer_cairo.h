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

#ifndef O3D_CORE_CROSS_CAIRO_RENDERER_CAIRO_H_
#define O3D_CORE_CROSS_CAIRO_RENDERER_CAIRO_H_

#include <cairo.h>
#include <build/build_config.h>
#include <list>
#include <vector>

#include "core/cross/renderer_platform.h"
#include "core/cross/renderer.h"
#include "core/cross/cairo/layer.h"

namespace o3d {

namespace o2d {

class RendererCairo : public Renderer {
 public:
  static RendererCairo* CreateDefault(ServiceLocator* service_locator);

  virtual ~RendererCairo();

  // Initializes stuff that has to happen after Init
  virtual void InitCommon();

  virtual void UninitCommon();

  // Initialises the renderer for use, claiming hardware resources.
  virtual InitStatus InitPlatformSpecific(const DisplayWindow& display,
                                          bool off_screen);

#ifdef OS_MACOSX
  virtual bool ChangeDisplayWindow(const DisplayWindow& display);

  virtual bool SupportsCoreGraphics() const { return true; }
#endif

  // Released all hardware resources.
  virtual void Destroy();

  // Paint the frame to the main view
  void Paint();

  // Insert the given Layer to the back of the array.
  void AddLayer(Layer* image);

  // Remove the given Layer from the array.
  void RemoveLayer(Layer* image);

  // Handles the plugin resize event.
  virtual void Resize(int width, int height);

  // Creates and returns a platform-specific RenderDepthStencilSurface object
  // for use as a depth-stencil render target.
  virtual RenderDepthStencilSurface::Ref CreateDepthStencilSurface(int width,
                                                                   int height);

  // Turns fullscreen display on.
  // Parameters:
  //  display: a platform-specific display identifier
  //  mode_id: a mode returned by GetDisplayModes
  // Returns true on success, false on failure.
  virtual bool GoFullscreen(const DisplayWindow& display,
                            int mode_id);

  // Cancels fullscreen display. Restores rendering to windowed mode
  // with the given width and height.
  // Parameters:
  //  display: a platform-specific display identifier
  //  width: the width to which to restore windowed rendering
  //  height: the height to which to restore windowed rendering
  // Returns true on success, false on failure.
  virtual bool CancelFullscreen(const DisplayWindow& display,
                                int width, int height);

  // Tells whether we're currently displayed fullscreen or not.
  virtual bool fullscreen() const;

  // Get a vector of the available fullscreen display modes.
  // Clears *modes on error.
  virtual void GetDisplayModes(std::vector<DisplayMode> *modes);

  // Get a single fullscreen display mode by id.
  // Returns true on success, false on error.
  virtual bool GetDisplayMode(int id, DisplayMode *mode);

  // Sets the state to the value of the param.
  // Parameters:
  //   renderer: the renderer
  //   param: param with state data
  virtual void SetState(Renderer* renderer, Param* param);

  // Creates a StreamBank, returning a platform specific implementation class.
  virtual StreamBank::Ref CreateStreamBank();

  // Creates a Primitive, returning a platform specific implementation class.
  virtual Primitive::Ref CreatePrimitive();

  // Creates a DrawElement, returning a platform specific implementation
  // class.
  virtual DrawElement::Ref CreateDrawElement();

  // Creates and returns a platform specific float buffer
  virtual VertexBuffer::Ref CreateVertexBuffer();

  // Creates and returns a platform specific integer buffer
  virtual IndexBuffer::Ref CreateIndexBuffer();

  // Creates and returns a platform specific effect object
  virtual Effect::Ref CreateEffect();

  // Creates and returns a platform specific Sampler object.
  virtual Sampler::Ref CreateSampler();

  // Returns a platform specific 4 element swizzle table for RGBA UByteN
  // fields.
  // The should contain the index of R, G, B, and A in that order for the
  // current platform.
  virtual const int* GetRGBAUByteNSwizzleTable();

  // Overriden from Renderer
  void PushRenderStates(State* state);

  // Overrider from Renderer
  void PopRenderStates();

 protected:
  typedef std::list<Layer*> LayerList;

  // Keep the constructor protected so only factory methods can create
  // renderers.
  explicit RendererCairo(ServiceLocator* service_locator);

  // Sets rendering to the back buffer.
  virtual void SetBackBufferPlatformSpecific();

  // Sets the render surfaces on a specific platform.
  virtual void SetRenderSurfacesPlatformSpecific(
      const RenderSurface* surface,
      const RenderDepthStencilSurface* depth_surface);

  // Creates a platform specific ParamCache.
  virtual ParamCache* CreatePlatformSpecificParamCache();

  // Platform specific version of CreateTexture2D
  virtual Texture2D::Ref CreatePlatformSpecificTexture2D(
      int width,
      int height,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

  // Platform specific version of CreateTextureCUBE.
  virtual TextureCUBE::Ref CreatePlatformSpecificTextureCUBE(
      int edge_length,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

  // The platform specific part of BeginDraw.
  virtual bool PlatformSpecificBeginDraw();

  // The platform specific part of EndDraw.
  virtual void PlatformSpecificEndDraw();

  // The platform specific part of StartRendering.
  virtual bool PlatformSpecificStartRendering();

  // The platform specific part of EndRendering.
  virtual void PlatformSpecificFinishRendering();

  // The platform specific part of Present.
  virtual void PlatformSpecificPresent();

  // The platform specific part of Clear.
  virtual void PlatformSpecificClear(const Float4 &color,
                                     bool color_flag,
                                     float depth,
                                     bool depth_flag,
                                     int stencil,
                                     bool stencil_flag);

  // Applies states that have been modified (marked dirty).
  virtual void ApplyDirtyStates();

  // Sets the viewport. This is the platform specific version.
  virtual void SetViewportInPixels(int left,
                                   int top,
                                   int width,
                                   int height,
                                   float min_z,
                                   float max_z);

  // Clip the area of the current layer that will collide with other images.
  void ClipArea(cairo_t* cr, LayerList::iterator it);

 private:
  void CreateCairoSurface();
  void DestroyCairoSurface();

#if defined(OS_LINUX)
  // Linux Client Display
  Display* display_;
  // Linux Client Window
  Window window_;
#elif defined(OS_MACOSX)
  CGContextRef mac_cg_context_ref_;
#elif defined(OS_WIN)
  HWND hwnd_;
#endif

  // Main surface to render cairo
  cairo_surface_t* main_surface_;

  // Array of Layer
  LayerList layer_list_;
};

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_RENDERER_CAIRO_H_
