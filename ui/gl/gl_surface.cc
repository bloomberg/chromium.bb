// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace gfx {

namespace {
base::LazyInstance<base::ThreadLocalPointer<GLSurface> >::Leaky
    current_surface_ = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
bool GLSurface::InitializeOneOff() {
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  TRACE_EVENT0("gpu,startup", "GLSurface::InitializeOneOff");

  std::vector<GLImplementation> allowed_impls;
  GetAllowedGLImplementations(&allowed_impls);
  DCHECK(!allowed_impls.empty());

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  // The default implementation is always the first one in list.
  GLImplementation impl = allowed_impls[0];
  bool fallback_to_osmesa = false;
  if (cmd->HasSwitch(switches::kOverrideUseGLWithOSMesaForTests)) {
    impl = kGLImplementationOSMesaGL;
  } else if (cmd->HasSwitch(switches::kUseGL)) {
    std::string requested_implementation_name =
        cmd->GetSwitchValueASCII(switches::kUseGL);
    if (requested_implementation_name == "any") {
      fallback_to_osmesa = true;
    } else if (requested_implementation_name ==
                   kGLImplementationSwiftShaderName ||
               requested_implementation_name == kGLImplementationANGLEName) {
      impl = kGLImplementationEGLGLES2;
    } else {
      impl = GetNamedGLImplementation(requested_implementation_name);
      if (!ContainsValue(allowed_impls, impl)) {
        LOG(ERROR) << "Requested GL implementation is not available.";
        return false;
      }
    }
  }

  bool gpu_service_logging = cmd->HasSwitch(switches::kEnableGPUServiceLogging);
  bool disable_gl_drawing = cmd->HasSwitch(switches::kDisableGLDrawingForTests);

  return InitializeOneOffImplementation(
      impl, fallback_to_osmesa, gpu_service_logging, disable_gl_drawing);
}

// static
bool GLSurface::InitializeOneOffImplementation(GLImplementation impl,
                                               bool fallback_to_osmesa,
                                               bool gpu_service_logging,
                                               bool disable_gl_drawing) {
  bool initialized =
      InitializeStaticGLBindings(impl) && InitializeOneOffInternal();
  if (!initialized && fallback_to_osmesa) {
    ClearGLBindings();
    initialized = InitializeStaticGLBindings(kGLImplementationOSMesaGL) &&
                  InitializeOneOffInternal();
  }
  if (!initialized)
    ClearGLBindings();

  if (initialized) {
    DVLOG(1) << "Using "
             << GetGLImplementationName(GetGLImplementation())
             << " GL implementation.";
    if (gpu_service_logging)
      InitializeDebugGLBindings();
    if (disable_gl_drawing)
      InitializeNullDrawGLBindings();
  }
  return initialized;
}

GLSurface::GLSurface() {}

bool GLSurface::Initialize() {
  return Initialize(SURFACE_DEFAULT);
}

bool GLSurface::Initialize(GLSurface::Format format) {
  return true;
}

bool GLSurface::Resize(const gfx::Size& size,
                       float scale_factor,
                       bool has_alpha) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::Recreate() {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::DeferDraws() {
  return false;
}

bool GLSurface::SupportsPostSubBuffer() {
  return false;
}

bool GLSurface::SupportsCommitOverlayPlanes() {
  return false;
}

bool GLSurface::SupportsAsyncSwap() {
  return false;
}

unsigned int GLSurface::GetBackingFrameBufferObject() {
  return 0;
}

void GLSurface::SwapBuffersAsync(const SwapCompletionCallback& callback) {
  NOTREACHED();
}

gfx::SwapResult GLSurface::PostSubBuffer(int x, int y, int width, int height) {
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurface::PostSubBufferAsync(int x,
                                   int y,
                                   int width,
                                   int height,
                                   const SwapCompletionCallback& callback) {
  NOTREACHED();
}

gfx::SwapResult GLSurface::CommitOverlayPlanes() {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurface::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& callback) {
  NOTREACHED();
}

bool GLSurface::OnMakeCurrent(GLContext* context) {
  return true;
}

bool GLSurface::SetBackbufferAllocation(bool allocated) {
  return true;
}

void GLSurface::SetFrontbufferAllocation(bool allocated) {
}

void* GLSurface::GetShareHandle() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetDisplay() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetConfig() {
  NOTIMPLEMENTED();
  return NULL;
}

GLSurface::Format GLSurface::GetFormat() {
  NOTIMPLEMENTED();
  return SURFACE_DEFAULT;
}

VSyncProvider* GLSurface::GetVSyncProvider() {
  return NULL;
}

bool GLSurface::ScheduleOverlayPlane(int z_order,
                                     OverlayTransform transform,
                                     gl::GLImage* image,
                                     const Rect& bounds_rect,
                                     const RectF& crop_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::ScheduleCALayer(gl::GLImage* contents_image,
                                const RectF& contents_rect,
                                float opacity,
                                unsigned background_color,
                                unsigned edge_aa_mask,
                                const RectF& rect,
                                bool is_clipped,
                                const RectF& clip_rect,
                                const Transform& transform,
                                int sorting_content_id,
                                unsigned filter) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::IsSurfaceless() const {
  return false;
}

bool GLSurface::FlipsVertically() const {
  return false;
}

bool GLSurface::BuffersFlipped() const {
  return false;
}

GLSurface* GLSurface::GetCurrent() {
  return current_surface_.Pointer()->Get();
}

GLSurface::~GLSurface() {
  if (GetCurrent() == this)
    SetCurrent(NULL);
}

void GLSurface::SetCurrent(GLSurface* surface) {
  current_surface_.Pointer()->Set(surface);
}

bool GLSurface::ExtensionsContain(const char* c_extensions, const char* name) {
  DCHECK(name);
  if (!c_extensions)
    return false;
  std::string extensions(c_extensions);
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

void GLSurface::OnSetSwapInterval(int interval) {
}

GLSurfaceAdapter::GLSurfaceAdapter(GLSurface* surface) : surface_(surface) {}

bool GLSurfaceAdapter::Initialize(GLSurface::Format format) {
  return surface_->Initialize(format);
}

void GLSurfaceAdapter::Destroy() {
  surface_->Destroy();
}

bool GLSurfaceAdapter::Resize(const gfx::Size& size,
                              float scale_factor,
                              bool has_alpha) {
  return surface_->Resize(size, scale_factor, has_alpha);
}

bool GLSurfaceAdapter::Recreate() {
  return surface_->Recreate();
}

bool GLSurfaceAdapter::DeferDraws() {
  return surface_->DeferDraws();
}

bool GLSurfaceAdapter::IsOffscreen() {
  return surface_->IsOffscreen();
}

gfx::SwapResult GLSurfaceAdapter::SwapBuffers() {
  return surface_->SwapBuffers();
}

void GLSurfaceAdapter::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  surface_->SwapBuffersAsync(callback);
}

gfx::SwapResult GLSurfaceAdapter::PostSubBuffer(int x,
                                                int y,
                                                int width,
                                                int height) {
  return surface_->PostSubBuffer(x, y, width, height);
}

void GLSurfaceAdapter::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& callback) {
  surface_->PostSubBufferAsync(x, y, width, height, callback);
}

gfx::SwapResult GLSurfaceAdapter::CommitOverlayPlanes() {
  return surface_->CommitOverlayPlanes();
}

void GLSurfaceAdapter::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& callback) {
  surface_->CommitOverlayPlanesAsync(callback);
}

bool GLSurfaceAdapter::SupportsPostSubBuffer() {
  return surface_->SupportsPostSubBuffer();
}

bool GLSurfaceAdapter::SupportsCommitOverlayPlanes() {
  return surface_->SupportsCommitOverlayPlanes();
}

bool GLSurfaceAdapter::SupportsAsyncSwap() {
  return surface_->SupportsAsyncSwap();
}

gfx::Size GLSurfaceAdapter::GetSize() {
  return surface_->GetSize();
}

void* GLSurfaceAdapter::GetHandle() {
  return surface_->GetHandle();
}

unsigned int GLSurfaceAdapter::GetBackingFrameBufferObject() {
  return surface_->GetBackingFrameBufferObject();
}

bool GLSurfaceAdapter::OnMakeCurrent(GLContext* context) {
  return surface_->OnMakeCurrent(context);
}

bool GLSurfaceAdapter::SetBackbufferAllocation(bool allocated) {
  return surface_->SetBackbufferAllocation(allocated);
}

void GLSurfaceAdapter::SetFrontbufferAllocation(bool allocated) {
  surface_->SetFrontbufferAllocation(allocated);
}

void* GLSurfaceAdapter::GetShareHandle() {
  return surface_->GetShareHandle();
}

void* GLSurfaceAdapter::GetDisplay() {
  return surface_->GetDisplay();
}

void* GLSurfaceAdapter::GetConfig() {
  return surface_->GetConfig();
}

GLSurface::Format GLSurfaceAdapter::GetFormat() {
  return surface_->GetFormat();
}

VSyncProvider* GLSurfaceAdapter::GetVSyncProvider() {
  return surface_->GetVSyncProvider();
}

bool GLSurfaceAdapter::ScheduleOverlayPlane(int z_order,
                                            OverlayTransform transform,
                                            gl::GLImage* image,
                                            const Rect& bounds_rect,
                                            const RectF& crop_rect) {
  return surface_->ScheduleOverlayPlane(
      z_order, transform, image, bounds_rect, crop_rect);
}

bool GLSurfaceAdapter::IsSurfaceless() const {
  return surface_->IsSurfaceless();
}

bool GLSurfaceAdapter::FlipsVertically() const {
  return surface_->FlipsVertically();
}

bool GLSurfaceAdapter::BuffersFlipped() const {
  return surface_->BuffersFlipped();
}

GLSurfaceAdapter::~GLSurfaceAdapter() {}

}  // namespace gfx
