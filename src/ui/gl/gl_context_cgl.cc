// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_cgl.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>

#include <memory>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_cgl.h"
#include "ui/gl/yuv_to_rgb_converter.h"

namespace gl {

namespace {

bool g_support_renderer_switching;

}  // namespace

static CGLPixelFormatObj GetPixelFormat() {
  static CGLPixelFormatObj format;
  if (format)
    return format;
  std::vector<CGLPixelFormatAttribute> attribs;
  // If the system supports dual gpus then allow offline renderers for every
  // context, so that they can all be in the same share group.
  if (GLContext::SwitchableGPUsSupported()) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
    g_support_renderer_switching = true;
  }
  if (GetGLImplementation() == kGLImplementationAppleGL) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back((CGLPixelFormatAttribute) kCGLRendererGenericFloatID);
    g_support_renderer_switching = false;
  }
  if (GetGLImplementation() == kGLImplementationDesktopGLCoreProfile) {
    // These constants don't exist in the 10.6 SDK against which
    // Chromium currently compiles.
    const int kOpenGLProfile = 99;
    const int kOpenGL3_2Core = 0x3200;
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(kOpenGLProfile));
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(kOpenGL3_2Core));
  }

  attribs.push_back((CGLPixelFormatAttribute) 0);

  GLint num_virtual_screens;
  if (CGLChoosePixelFormat(&attribs.front(),
                           &format,
                           &num_virtual_screens) != kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return nullptr;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return nullptr;
  }
  DCHECK_NE(num_virtual_screens, 0);
  return format;
}

GLContextCGL::GLContextCGL(GLShareGroup* share_group)
    : GLContextReal(share_group) {}

bool GLContextCGL::Initialize(GLSurface* compatible_surface,
                              const GLContextAttribs& attribs) {
  DCHECK(compatible_surface);

  // webgl_compatibility_context and disabling bind_generates_resource are not
  // supported.
  DCHECK(!attribs.webgl_compatibility_context &&
         attribs.bind_generates_resource);

  GpuPreference gpu_preference =
      GLContext::AdjustGpuPreference(attribs.gpu_preference);

  GLContextCGL* share_context = share_group() ?
      static_cast<GLContextCGL*>(share_group()->GetContext()) : nullptr;

  CGLPixelFormatObj format = GetPixelFormat();
  if (!format)
    return false;

  // If using the discrete gpu, create a pixel format requiring it before we
  // create the context.
  if (!GLContext::SwitchableGPUsSupported() ||
      gpu_preference == GpuPreference::kHighPerformance) {
    std::vector<CGLPixelFormatAttribute> discrete_attribs;
    discrete_attribs.push_back((CGLPixelFormatAttribute) 0);
    GLint num_pixel_formats;
    if (CGLChoosePixelFormat(&discrete_attribs.front(),
                             &discrete_pixelformat_,
                             &num_pixel_formats) != kCGLNoError) {
      LOG(ERROR) << "Error choosing pixel format.";
      return false;
    }
    // The renderer might be switched after this, so ignore the saved ID.
    share_group()->SetRendererID(-1);
  }

  CGLError res = CGLCreateContext(
      format,
      share_context ?
          static_cast<CGLContextObj>(share_context->GetHandle()) : nullptr,
      reinterpret_cast<CGLContextObj*>(&context_));
  if (res != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }

  gpu_preference_ = gpu_preference;
  // Contexts that prefer low power gpu are known to use only the subset of GL
  // that can be safely migrated between the iGPU and the dGPU. Mark those
  // contexts as safe to forcibly transition between the GPUs by default.
  // http://crbug.com/180876, http://crbug.com/227228
  safe_to_force_gpu_switch_ = gpu_preference == GpuPreference::kLowPower;
  return true;
}

void GLContextCGL::Destroy() {
  if (!yuv_to_rgb_converters_.empty() || !backpressure_fences_.empty()) {
    // If this context is not current, bind this context's API so that the YUV
    // converter and GLFences can safely destruct
    GLContext* current_context = GetRealCurrent();
    if (current_context != this) {
      SetCurrentGL(GetCurrentGL());
    }

    ScopedCGLSetCurrentContext scoped_set_current(
        static_cast<CGLContextObj>(context_));
    yuv_to_rgb_converters_.clear();
    backpressure_fences_.clear();

    // Rebind the current context's API if needed.
    if (current_context && current_context != this) {
      SetCurrentGL(current_context->GetCurrentGL());
    }
  }
  if (discrete_pixelformat_) {
    if (base::ThreadTaskRunnerHandle::IsSet()) {
      // Delay releasing the pixel format for 10 seconds to reduce the number of
      // unnecessary GPU switches.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&CGLReleasePixelFormat, discrete_pixelformat_),
          base::TimeDelta::FromSeconds(10));
    } else {
      CGLReleasePixelFormat(discrete_pixelformat_);
    }
    discrete_pixelformat_ = nullptr;
  }
  if (context_) {
    CGLDestroyContext(static_cast<CGLContextObj>(context_));
    context_ = nullptr;
  }
}

bool GLContextCGL::ForceGpuSwitchIfNeeded() {
  DCHECK(context_);

  // The call to CGLSetVirtualScreen can hang on some AMD drivers
  // http://crbug.com/227228
  if (safe_to_force_gpu_switch_) {
    int renderer_id = share_group()->GetRendererID();
    int screen;
    CGLGetVirtualScreen(static_cast<CGLContextObj>(context_), &screen);

    if (g_support_renderer_switching &&
        !discrete_pixelformat_ && renderer_id != -1 &&
        (screen != screen_ || renderer_id != renderer_id_)) {
      // Attempt to find a virtual screen that's using the requested renderer,
      // and switch the context to use that screen. Don't attempt to switch if
      // the context requires the discrete GPU.
      CGLPixelFormatObj format = GetPixelFormat();
      int virtual_screen_count;
      if (CGLDescribePixelFormat(format, 0, kCGLPFAVirtualScreenCount,
                                 &virtual_screen_count) != kCGLNoError)
        return false;

      for (int i = 0; i < virtual_screen_count; ++i) {
        int screen_renderer_id;
        if (CGLDescribePixelFormat(format, i, kCGLPFARendererID,
                                   &screen_renderer_id) != kCGLNoError)
          return false;

        screen_renderer_id &= kCGLRendererIDMatchingMask;
        if (screen_renderer_id == renderer_id) {
          CGLSetVirtualScreen(static_cast<CGLContextObj>(context_), i);
          screen_ = i;
          break;
        }
      }
      renderer_id_ = renderer_id;
      has_switched_gpus_ = true;
    }
  }
  return true;
}

YUVToRGBConverter* GLContextCGL::GetYUVToRGBConverter(
    const gfx::ColorSpace& color_space) {
  std::unique_ptr<YUVToRGBConverter>& yuv_to_rgb_converter =
      yuv_to_rgb_converters_[color_space];
  if (!yuv_to_rgb_converter)
    yuv_to_rgb_converter =
        std::make_unique<YUVToRGBConverter>(*GetVersionInfo(), color_space);
  return yuv_to_rgb_converter.get();
}

constexpr uint64_t kInvalidFenceId = 0;

uint64_t GLContextCGL::BackpressureFenceCreate() {
  TRACE_EVENT0("gpu", "GLContextCGL::BackpressureFenceCreate");

  // This flush will trigger a crash if FlushForDriverCrashWorkaround is not
  // called sufficiently frequently.
  glFlush();

  if (gl::GLFence::IsSupported()) {
    next_backpressure_fence_ += 1;
    backpressure_fences_[next_backpressure_fence_] = GLFence::Create();
    return next_backpressure_fence_;
  }
  glFinish();
  return kInvalidFenceId;
}

void GLContextCGL::BackpressureFenceWait(uint64_t fence_id) {
  TRACE_EVENT0("gpu", "GLContextCGL::BackpressureFenceWait");
  if (fence_id == kInvalidFenceId) {
    return;
  }

  // If a fence is not found, then it has already been waited on.
  auto found = backpressure_fences_.find(fence_id);
  if (found == backpressure_fences_.end())
    return;
  std::unique_ptr<GLFence> fence = std::move(found->second);
  backpressure_fences_.erase(found);

  // While we could call GLFence::ClientWait, this performs a busy wait on
  // Mac, leading to high CPU usage. Instead we poll with a 1ms delay. This
  // should have minimal impact, as we will only hit this path when we are
  // more than one frame (16ms) behind.
  //
  // Note that on some platforms (10.9), fences appear to sometimes get
  // lost and will never pass. Add a 32ms timout to prevent these
  // situations from causing a GPU process hang.
  // https://crbug.com/618075
  bool fence_completed = false;
  for (int poll_iter = 0; !fence_completed && poll_iter < 32; ++poll_iter) {
    if (poll_iter > 0) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
    {
      TRACE_EVENT0("gpu", "GLFence::HasCompleted");
      fence_completed = fence->HasCompleted();
    }
  }
  if (!fence_completed) {
    TRACE_EVENT0("gpu", "Finish");
    // We timed out waiting for the above fence, just issue a glFinish.
    glFinish();
  }
  fence.reset();

  // Waiting on |fence_id| has implicitly waited on all previous fences, so
  // remove them.
  while (backpressure_fences_.begin()->first < fence_id)
    backpressure_fences_.erase(backpressure_fences_.begin());
}

void GLContextCGL::FlushForDriverCrashWorkaround() {
  if (!context_ || CGLGetCurrentContext() != context_)
    return;
  glFlush();
}

bool GLContextCGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);

  if (!ForceGpuSwitchIfNeeded())
    return false;

  if (IsCurrent(surface))
    return true;

  // It's likely we're going to switch OpenGL contexts at this point.
  // Before doing so, if there is a current context, flush it. There
  // are many implicit assumptions of flush ordering between contexts
  // at higher levels, and if a flush isn't performed, OpenGL commands
  // may be issued in unexpected orders, causing flickering and other
  // artifacts.
  if (CGLGetCurrentContext() != nullptr) {
    glFlush();
  }

  ScopedReleaseCurrent release_current;
  TRACE_EVENT0("gpu", "GLContextCGL::MakeCurrent");

  if (CGLSetCurrentContext(
      static_cast<CGLContextObj>(context_)) != kCGLNoError) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  // Set this as soon as the context is current, since we might call into GL.
  BindGLApi();

  SetCurrent(surface);
  InitializeDynamicBindings();

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  release_current.Cancel();
  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  // Before releasing the current context, flush it. This ensures that
  // all commands issued by higher levels will be seen by the OpenGL
  // implementation, which is assumed throughout the code. See comment
  // in MakeCurrent, above.
  glFlush();

  SetCurrent(nullptr);
  CGLSetCurrentContext(nullptr);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current = CGLGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetRealCurrent() == this));

  if (!native_context_is_current)
    return false;

  return true;
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSafeToForceGpuSwitch() {
  safe_to_force_gpu_switch_ = true;
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

GpuPreference GLContextCGL::GetGpuPreference() {
  return gpu_preference_;
}

}  // namespace gl
