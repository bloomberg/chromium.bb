/*
 * Copyright 2011, Google Inc.
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

// A 2D renderer that uses the Cairo library.

#include "core/cross/cairo/renderer_cairo.h"

#if defined(OS_LINUX)
#include <cairo-xlib.h>
#elif defined(OS_MACOSX)
#include <cairo-quartz.h>
#elif defined(OS_WIN)
#include <cairo-win32-private.h>
#include <cairo-win32.h>
#endif

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/cross/cairo/layer.h"
#include "core/cross/cairo/texture_cairo.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
// As of OS X 10.6.4, the Quartz 2D drawing API has hardware acceleration
// disabled by default, and if you force-enable it then it actually hurts
// performance instead of improving it (really, go google it). It also turns out
// that the performance of the software implementation in Pixman is about 12%
// faster than the OS X software implementation (measured as CPU usage per
// rendered frame), so we do all compositing with Pixman via an image surface
// and only use the OS to paint the final frame to the screen.

// On Windows COMPOSITING_TO_IMAGE is also slightly faster than compositing with
// GDI.
// TODO(tschmelcher): Profile Windows without COMPOSITING_TO_IMAGE and see if we
// can make it faster.
#define COMPOSITING_TO_IMAGE 1
#endif

namespace o3d {

// This is a factory function for creating 2D Renderer objects.
Renderer* Renderer::Create2DRenderer(ServiceLocator* service_locator) {
  return o2d::RendererCairo::CreateDefault(service_locator);
}

namespace o2d {

RendererCairo::RendererCairo(ServiceLocator* service_locator)
    : Renderer(service_locator),
#if defined(OS_LINUX)
      display_(NULL),
      window_(0),
#elif defined(OS_MACOSX)
      mac_cg_context_ref_(0),
#elif defined(OS_WIN)
      hwnd_(NULL),
#endif
      display_surface_(NULL),
      offscreen_surface_(NULL),
      fullscreen_(false),
      size_dirty_(false) {
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

  DestroyOffscreenSurface();
#if defined(OS_LINUX) || defined(OS_WIN)
  DestroyDisplaySurface();
#endif

#if defined(OS_LINUX)
  display_ = NULL;
  window_ = 0;
#elif defined(OS_MACOSX)
  mac_cg_context_ref_ = 0;
#elif defined(OS_WIN)
  hwnd_ = NULL;
#endif
}

// Comparison predicate for STL sort.
static bool LayerZValueLessThan(const Layer* first, const Layer* second) {
  return first->z() < second->z();
}

static void AddRectangleRegion(cairo_t* cr, const Layer::Region& region) {
  cairo_rectangle(cr,
                  region.x,
                  region.y,
                  region.width,
                  region.height);
}

typedef double (*rounding_fn)(double);

static void RoundRegion(const Layer::Region& in_region,
                        rounding_fn round_x1y1,
                        rounding_fn round_x2y2,
                        Layer::Region* out_region) {
  out_region->everywhere = in_region.everywhere;
  out_region->x = (*round_x1y1)(in_region.x);
  out_region->y = (*round_x1y1)(in_region.y);
  out_region->width = (*round_x2y2)(in_region.x + in_region.width)
      - out_region->x;
  out_region->height = (*round_x2y2)(in_region.y + in_region.height)
      - out_region->y;
}

void RendererCairo::Paint() {
#ifdef OS_MACOSX
  // On OSX we can't persist the display surface across Paint() calls because
  // of issues with unbalanced CGContextSaveGState/RestoreGState calls, so we
  // have to create and destroy the surface for every frame.
  CreateDisplaySurface();
#endif

#if !defined(COMPOSITING_TO_IMAGE) && defined(OS_LINUX)
  if (!offscreen_surface_) {
    // Have to call this here because strangely it breaks rendering if we call
    // it during InitCommon() on Linux. Possibly the X11 Window underlying the
    // display_surface_ is not fully initialized until sometime after
    // NPP_SetWindow().
    CreateOffscreenSurface();
  }
#endif

  if (!display_surface_ || !offscreen_surface_) {
    DLOG(INFO) << "No target surface(s), cannot paint";
    return;
  }

  // TODO(tschmelcher): Don't keep creating and destroying the drawing context.
  cairo_t* cr = cairo_create(offscreen_surface_);

  cairo_save(cr);

  // Set clip rule for building the initial clip region.
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

  // If we have been resized then we must redraw everything, since the new
  // offscreen surface will be uninitialized.
  if (size_dirty_) {
    AddDisplayRegion(cr);
  }

  // Build an initial clip region for all drawing operations that restricts
  // drawing to only areas of the screen that have changed.
  bool needs_sort = false;
  for (LayerList::const_iterator i = layer_list_.begin();
       i != layer_list_.end(); i++) {
    Layer* layer = *i;

    // Aggregate all content dirtiness into the layer's dirtiness property.
    Pattern* pattern = layer->pattern();
    if (pattern) {
      if (pattern->content_dirty()) {
        layer->set_content_dirty(true);
      } else {
        TextureCairo* texture = pattern->texture();
        if (texture && texture->content_dirty()) {
          layer->set_content_dirty(true);
        }
      }
    }

    if (!layer->ShouldPaint() && !layer->GetSavedShouldPaint()) {
      // Won't be painted now and wasn't painted on the last frame either, so
      // doesn't need to be redrawn.
      continue;
    }

    if (layer->ShouldPaint() && layer->z_dirty()) {
      // If the z-value has changed and it is going to be painted then we need
      // to re-sort.
      needs_sort = true;
    }

    if (layer->region_dirty()) {
      // Recompute clip regions.
      RoundRegion(layer->region(), &ceil, &floor, &layer->inner_clip_region());
      RoundRegion(layer->region(), &floor, &ceil, &layer->outer_clip_region());
    }

    if (layer->ShouldPaint() && (layer->region_dirty() ||
                                 layer->content_dirty() ||
                                 !layer->GetSavedShouldPaint())) {
      // If it is visible and the region, content, or visibility has changed
      // then the current region must be redrawn.
      AddRegion(cr, layer->outer_clip_region());
    }

    if (layer->GetSavedShouldPaint() && (layer->region_dirty() ||
                                         !layer->ShouldPaint())) {
      // If the region or visibility has changed and it was visible before then
      // the old region must be redrawn.
      AddRegion(cr, layer->GetSavedOuterClipRegion());
    }
  }

  // Must also redraw the regions of any formerly visible layers.
  for (RegionList::const_iterator i = layer_ghost_list_.begin();
       i != layer_ghost_list_.end(); ++i) {
    AddRegion(cr, *i);
  }

  // Clip everything outside the regions defined by the above.
  cairo_clip(cr);

  // If any visible layer has had its z-value modified, re-sort the list to get
  // the right draw order.
  if (needs_sort) {
    std::sort(layer_list_.begin(), layer_list_.end(), LayerZValueLessThan);
  }

  // Change clip rule for the paint operations.
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

  // Core process of painting.
  for (LayerList::const_iterator i = layer_list_.begin();
       i != layer_list_.end(); i++) {
    Layer* layer = *i;
    if (!layer->ShouldPaint()) continue;

    Pattern* pattern = layer->pattern();

    // Save the current drawing state.
    cairo_save(cr);

    // Clip the region outside the current Layer.
    if (!layer->everywhere()) {
      // This should really be layer->region(), but there is a bug in cairo with
      // complex unaligned clip regions so we use an aligned one instead. See
      // http://lists.cairographics.org/archives/cairo/2011-March/021827.html
      // This doesn't really matter though because any sensible web app will
      // align everything to pixel boundaries.
      AddRectangleRegion(cr, layer->outer_clip_region());
      cairo_clip(cr);
    }

    // Clip the regions within other Layers that will obscure this one.
    LayerList::const_iterator start_mask_it = i;
    start_mask_it++;
    ClipArea(cr, start_mask_it);

    // Transform the pattern to fit into the Layer's region.
    cairo_translate(cr, layer->x(), layer->y());
    cairo_scale(cr, layer->scale_x(), layer->scale_y());

    // Set source pattern.
    cairo_set_source(cr, pattern->pattern());

    // Paint the pattern to the off-screen surface.
    switch (layer->paint_operator()) {
      case Layer::BLEND:
        cairo_paint(cr);
        break;

      case Layer::BLEND_WITH_TRANSPARENCY:
        cairo_paint_with_alpha(cr, layer->alpha());
        break;

      case Layer::COPY:
        // Set Cairo to copy the pattern's alpha content instead of blending.
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        break;

      case Layer::COPY_WITH_FADING:
        // TODO(tschmelcher): This can also be done in a single operation with:
        //
        //   cairo_set_operator(cr, CAIRO_OPERATOR_IN);
        //   cairo_paint_with_alpha(cr, layer->alpha());
        //
        // but surprisingly that is slightly slower for me. We should figure out
        // why.
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_paint_with_alpha(cr, layer->alpha());
        break;

      default:
        DCHECK(false);
    }

    // Restore to a clean state.
    cairo_restore(cr);
  }

  cairo_restore(cr);

  // Finish drawing to the offscreen surface.
  cairo_destroy(cr);

  // Set up a new context for painting the offscreen surface to the screen.
  cr = cairo_create(display_surface_);
  cairo_set_source_surface(cr, offscreen_surface_, 0, 0);
  // Painting to a Win32 window with the SOURCE operator interacts poorly with
  // window resizing, so on Windows we use the default of OVER.
#ifndef OS_WIN
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
#endif

  // Paint to the screen.
  // TODO(tschmelcher): We can clip here too if we know that the on-screen
  // window hasn't been damaged by another window.
  cairo_paint(cr);

  cairo_destroy(cr);

  // Clear dirtiness.

  size_dirty_ = false;

  for (LayerList::const_iterator i = layer_list_.begin();
       i != layer_list_.end(); i++) {
    Layer* layer = *i;

    // Clear pattern/texture content dirtiness on all layers, since it has been
    // aggregated into the layer.
    Pattern* pattern = layer->pattern();
    if (pattern) {
      pattern->set_content_dirty(false);
      TextureCairo* texture = pattern->texture();
      if (texture) {
        texture->set_content_dirty(false);
      }
    }

    if (needs_sort) {
      // If we sorted the list, clear the z dirty flag for everyone, since even
      // layers that were not updated will have been sorted into the right
      // order.
      layer->set_z_dirty(false);
    }

    if (!layer->ShouldPaint() && !layer->GetSavedShouldPaint()) {
      // Was not updated, so any layer dirtiness persists.
      continue;
    }

    // Clear layer dirtiness.
    layer->set_region_dirty(false);
    layer->set_content_dirty(false);

    // Save current region and paintability for reference on the next frame.
    layer->SaveShouldPaint();
    layer->SaveOuterClipRegion();
  }

  layer_ghost_list_.clear();

#ifdef OS_MACOSX
  DestroyDisplaySurface();
#endif
}

void RendererCairo::ClipArea(cairo_t* cr,  LayerList::const_iterator it) {
  for (LayerList::const_iterator i = it; i != layer_list_.end(); i++) {
    // Preparing and updating the Layer.
    Layer* layer = *i;
    if (!layer->ShouldClip()) continue;

    if (!layer->everywhere()) {
      AddDisplayRegion(cr);
      AddRectangleRegion(cr, layer->inner_clip_region());
    }
    cairo_clip(cr);
  }
}

void RendererCairo::AddLayer(Layer* layer) {
  layer_list_.push_back(layer);
}

void RendererCairo::RemoveLayer(Layer* layer) {
  LayerList::iterator i = std::find(layer_list_.begin(),
                                    layer_list_.end(),
                                    layer);
  if (layer_list_.end() == i) {
    return;
  }
  layer_list_.erase(i);
  if (layer->GetSavedShouldPaint()) {
    // Need to remember this layer's region so we can redraw it on the next
    // frame.
    layer_ghost_list_.push_back(layer->GetSavedOuterClipRegion());
  }
}

void RendererCairo::InitCommon() {
#if defined(OS_LINUX) || defined(OS_WIN)
  CreateDisplaySurface();
#endif
#if defined(COMPOSITING_TO_IMAGE) || !defined(OS_LINUX)
  CreateOffscreenSurface();
#endif
}

void RendererCairo::CreateDisplaySurface() {
#if defined(OS_LINUX)
  display_surface_ = cairo_xlib_surface_create(display_,
                                               window_,
                                               XDefaultVisual(display_, 0),
                                               display_width(),
                                               display_height());
#elif defined(OS_MACOSX)
  if (!mac_cg_context_ref_) {
    // Can't create the surface until we get a valid CGContextRef. If we don't
    // have one it may be because we haven't yet gotten one from
    // HandleCocoaEvent() in main_mac.mm, or we may have initialized with a
    // drawing model other than Core Graphics (in which case we are never going
    // to get one and rendering won't work).
    // TODO(tschmelcher): Support the other drawing models too.
    DLOG(INFO) << "No CGContextRef, cannot initialize yet";
    return;
  }
  // Normally this function requires the caller to transform the CG coordinate
  // space to match Cairo's origin convention, but in NPAPI it happens to
  // already be that way.
  display_surface_ = cairo_quartz_surface_create_for_cg_context(
      mac_cg_context_ref_,
      display_width(),
      display_height());
#elif defined(OS_WIN)
  HDC hdc = GetDC(hwnd_);

  display_surface_ = cairo_win32_surface_create(hdc);
#endif

  if (CAIRO_STATUS_SUCCESS != cairo_surface_status(display_surface_)) {
    DLOG(ERROR) << "Failed to create display surface";
    DestroyDisplaySurface();
    return;
  }

#ifdef OS_WIN
  // Check the surface to make sure it has the correct clip box.
  cairo_win32_surface_t* cairo_surface =
      reinterpret_cast<cairo_win32_surface_t*>(display_surface_);
  if (cairo_surface->extents.width != display_width() ||
      cairo_surface->extents.height != display_height()) {
    // The clip box doesn't have the right info.  Need to do the following:
    // 1. Update the surface parameters to the right rectangle.
    // 2. Try to update the DC clip region.
    DLOG(WARNING) << "CreateDisplaySurface updates clip box from (0,0,"
                  << cairo_surface->extents.width << ","
                  << cairo_surface->extents.height << ") to (0,0,"
                  << display_width() << "," << display_height() << ").";
    HRGN hRegion = CreateRectRgn(0, 0, display_width(), display_height());
    int result = SelectClipRgn(hdc, hRegion);
    if (result == ERROR) {
      LOG(ERROR) << "CreateDisplaySurface SelectClipRgn returns ERROR";
    } else if (result == NULLREGION) {
      // This should not happen since the ShowWindow is called.
      LOG(ERROR) << "CreateDisplaySurface SelectClipRgn returns NULLREGION";
    }
    DeleteObject(hRegion);
    cairo_surface->extents.width = display_width();
    cairo_surface->extents.height = display_height();
  }

  ReleaseDC(hwnd_, hdc);
#endif
}

void RendererCairo::DestroyDisplaySurface() {
  if (display_surface_ != NULL) {
    cairo_surface_destroy(display_surface_);
    display_surface_ = NULL;
  }
}

void RendererCairo::CreateOffscreenSurface() {
  offscreen_surface_ = CreateSimilarSurface(CAIRO_CONTENT_COLOR,
                                            display_width(),
                                            display_height());
  if (!offscreen_surface_) {
    DLOG(ERROR) << "Failed to create offscreen surface";
  }
}

void RendererCairo::DestroyOffscreenSurface() {
  if (offscreen_surface_) {
    cairo_surface_destroy(offscreen_surface_);
    offscreen_surface_ = NULL;
  }
}

#if defined(COMPOSITING_TO_IMAGE) || defined(OS_MACOSX)
static cairo_format_t CairoFormatFromCairoContent(cairo_content_t content) {
  switch (content) {
    case CAIRO_CONTENT_COLOR:
      return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
      return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
      return CAIRO_FORMAT_ARGB32;
    default:
      DCHECK(false);
      return CAIRO_FORMAT_ARGB32;
  }
}
#endif

cairo_surface_t* RendererCairo::CreateSimilarSurface(cairo_content_t content,
                                                     int width,
                                                     int height) {
  cairo_surface_t* similar;
#if defined(COMPOSITING_TO_IMAGE)
  similar = cairo_image_surface_create(CairoFormatFromCairoContent(content),
                                       width,
                                       height);
#elif defined(OS_LINUX) || defined(OS_WIN)
  if (!display_surface_) {
    DLOG(INFO) << "No display surface, cannot create similar surface";
    return NULL;
  }
  similar = cairo_surface_create_similar(display_surface_,
                                         content,
                                         width,
                                         height);
#else  // OS_MACOSX
  // On OSX we can't use cairo_surface_create_similar() because display_surface_
  // is only valid during Paint(), so instead hard-code what a
  // cairo_surface_create_similar() call would do. Conveniently the OSX
  // implementation doesn't actually make use of the surface argument.
  // (Note that this code path is not actually taken right now because we use
  // COMPOSITING_TO_IMAGE on OSX.)
  similar = cairo_quartz_surface_create(CairoFormatFromCairoContent(content),
                                        width,
                                        height);
#endif

  if (CAIRO_STATUS_SUCCESS != cairo_surface_status(similar)) {
    DLOG(ERROR) << "Failed to create similar surface";
    cairo_surface_destroy(similar);
    similar = NULL;
  }

  return similar;
}

void RendererCairo::AddDisplayRegion(cairo_t* cr) {
  cairo_rectangle(cr, 0, 0, display_width(), display_height());
}

void RendererCairo::AddRegion(cairo_t* cr, const Layer::Region& region) {
  if (region.everywhere) {
    AddDisplayRegion(cr);
  } else {
    AddRectangleRegion(cr, region);
  }
}

void RendererCairo::UninitCommon() {
  // Don't need to do anything.
}

Renderer::InitStatus RendererCairo::InitPlatformSpecific(
    const DisplayWindow& display_window,
    bool off_screen) {
#if defined(OS_LINUX)
  const DisplayWindowLinux &display_platform =
      static_cast<const DisplayWindowLinux&>(display_window);
  display_ = display_platform.display();
  window_ = display_platform.window();
#elif defined(OS_MACOSX)
  const DisplayWindowMac &display_platform =
      static_cast<const DisplayWindowMac&>(display_window);
  mac_cg_context_ref_ = display_platform.cg_context_ref();
#elif defined(OS_WIN)
  const DisplayWindowWindows &display_platform =
      static_cast<const DisplayWindowWindows&>(display_window);
  hwnd_ = display_platform.hwnd();
  if (NULL == hwnd_) {
    return INITIALIZATION_ERROR;
  }
#endif

  return SUCCESS;
}

#ifdef OS_MACOSX
bool RendererCairo::ChangeDisplayWindow(const DisplayWindow& display_window) {
  const DisplayWindowMac &display_platform =
      static_cast<const DisplayWindowMac&>(display_window);
  mac_cg_context_ref_ = display_platform.cg_context_ref();
}
#endif

// Handles the plugin resize event.
void RendererCairo::Resize(int width, int height) {
  DLOG(INFO) << "To Resize " << width << " x " << height;
  SetClientSize(width, height);

#if defined(OS_LINUX)
  if (display_surface_) {
    cairo_xlib_surface_set_size(display_surface_, width, height);
  }
#elif defined(OS_WIN)
  DestroyDisplaySurface();
  CreateDisplaySurface();
#endif

  DestroyOffscreenSurface();
#if defined(COMPOSITING_TO_IMAGE) || !defined(OS_LINUX)
  CreateOffscreenSurface();
#endif
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

// Turns fullscreen display on.
// Parameters:
//  display: a platform-specific display identifier
//  mode_id: a mode returned by GetDisplayModes
// Returns true on success, false on failure.
bool RendererCairo::GoFullscreen(const DisplayWindow& display,
                                 int mode_id) {
  if (!fullscreen_) {
    DLOG(INFO) << "RendererCairo entering fullscreen";
#if defined(OS_LINUX)
    const DisplayWindowLinux &display_platform =
        static_cast<const DisplayWindowLinux&>(display);
    display_ = display_platform.display();
    window_ = display_platform.window();
    XWindowAttributes window_attributes;
    if (XGetWindowAttributes(display_, window_, &window_attributes)) {
      fullscreen_ = true;
      SetClientSize(window_attributes.width, window_attributes.height);
      DestroyDisplaySurface();
      CreateDisplaySurface();
      DestroyOffscreenSurface();
#ifdef COMPOSITING_TO_IMAGE
      CreateOffscreenSurface();
#endif
    }
#elif defined(OS_WIN)
    DEVMODE dev_mode;
    if (hwnd_ != NULL &&
        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode)) {
      int width = dev_mode.dmPelsWidth;
      int height = dev_mode.dmPelsHeight;

      fullscreen_ = true;
      Resize(width, height);
    }
#endif
  }
  return fullscreen_;
}

// Cancels fullscreen display. Restores rendering to windowed mode
// with the given width and height.
// Parameters:
//  display: a platform-specific display identifier
//  width: the width to which to restore windowed rendering
//  height: the height to which to restore windowed rendering
// Returns true on success, false on failure.
bool RendererCairo::CancelFullscreen(const DisplayWindow& display,
                                     int width, int height) {
  if (fullscreen_) {
    DLOG(INFO) << "RendererCairo exiting fullscreen";
#if defined(OS_LINUX)
    const DisplayWindowLinux &display_platform =
        static_cast<const DisplayWindowLinux&>(display);
    display_ = display_platform.display();
    window_ = display_platform.window();

    fullscreen_ = false;
    SetClientSize(width, height);
    DestroyDisplaySurface();
    CreateDisplaySurface();
    DestroyOffscreenSurface();
#ifdef COMPOSITING_TO_IMAGE
    CreateOffscreenSurface();
#endif
#elif defined(OS_WIN)
    if (hwnd_ == NULL) {
      // Not initialized.
      return false;
    }
    fullscreen_ = false;
    Resize(width, height);
#endif
  }
  return true;
}

// TODO(fransiskusx): Need to implement it later.
// Tells whether we're currently displayed fullscreen or not.
bool RendererCairo::fullscreen() const {
  return fullscreen_;
}

// Sets the client's size. Overridden from Renderer.
void RendererCairo::SetClientSize(int width, int height) {
  Renderer::SetClientSize(width, height);
  size_dirty_ = true;
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

void RendererCairo::PopRenderStates() {
  NOTIMPLEMENTED();
}

}  // namespace o2d

}  // namespace o3d
