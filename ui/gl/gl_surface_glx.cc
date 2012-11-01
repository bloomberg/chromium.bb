// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gl/gl_surface_glx.h"

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/time.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/base/x/x11_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {

namespace {

// scoped_ptr functor for XFree(). Use as follows:
//   scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> foo(...);
// where "XVisualInfo" is any X type that is freed with XFree.
class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

Display* g_display;
const char* g_glx_extensions = NULL;
bool g_glx_create_context_robustness_supported = false;
bool g_glx_texture_from_pixmap_supported = false;
bool g_glx_oml_sync_control_supported = false;

// Track support of glXGetMscRateOML separately from GLX_OML_sync_control as a
// whole since on some platforms (e.g. crosbug.com/34585), glXGetMscRateOML
// always fails even though GLX_OML_sync_control is reported as being supported.
bool g_glx_get_msc_rate_oml_supported = false;

}  // namespace

GLSurfaceGLX::GLSurfaceGLX() {}

bool GLSurfaceGLX::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  g_display = base::MessagePumpForUI::GetDefaultXDisplay();
  if (!g_display) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  int major, minor;
  if (!glXQueryVersion(g_display, &major, &minor)) {
    LOG(ERROR) << "glxQueryVersion failed";
    return false;
  }

  if (major == 1 && minor < 3) {
    LOG(ERROR) << "GLX 1.3 or later is required.";
    return false;
  }

  g_glx_extensions = glXQueryExtensionsString(g_display, 0);
  g_glx_create_context_robustness_supported =
      HasGLXExtension("GLX_ARB_create_context_robustness");
  g_glx_texture_from_pixmap_supported =
      HasGLXExtension("GLX_EXT_texture_from_pixmap");
  g_glx_oml_sync_control_supported =
      HasGLXExtension("GLX_OML_sync_control");
  g_glx_get_msc_rate_oml_supported = g_glx_oml_sync_control_supported;


  initialized = true;
  return true;
}

// static
const char* GLSurfaceGLX::GetGLXExtensions() {
  return g_glx_extensions;
}

// static
bool GLSurfaceGLX::HasGLXExtension(const char* name) {
  return ExtensionsContain(GetGLXExtensions(), name);
}

// static
bool GLSurfaceGLX::IsCreateContextRobustnessSupported() {
  return g_glx_create_context_robustness_supported;
}

// static
bool GLSurfaceGLX::IsTextureFromPixmapSupported() {
  return g_glx_texture_from_pixmap_supported;
}

// static
bool GLSurfaceGLX::IsOMLSyncControlSupported() {
  return g_glx_oml_sync_control_supported;
}

void* GLSurfaceGLX::GetDisplay() {
  return g_display;
}

GLSurfaceGLX::~GLSurfaceGLX() {}

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window)
  : window_(window),
    config_(NULL) {
}

bool NativeViewGLSurfaceGLX::Initialize() {
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(g_display, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }
  size_ = gfx::Size(attributes.width, attributes.height);
  return true;
}

void NativeViewGLSurfaceGLX::Destroy() {
}

bool NativeViewGLSurfaceGLX::Resize(const gfx::Size& size) {
  // On Intel drivers, the frame buffer won't be resize until the next swap. If
  // we only do PostSubBuffer, then we're stuck in the old size. Force a swap
  // now.
  if (gfx::g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer && size_ != size)
    SwapBuffers();
  size_ = size;
  return true;
}

bool NativeViewGLSurfaceGLX::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceGLX::SwapBuffers() {
  glXSwapBuffers(g_display, window_);
  // For latency_tests.cc:
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");
  return true;
}

gfx::Size NativeViewGLSurfaceGLX::GetSize() {
  return size_;
}

void* NativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(window_);
}

std::string NativeViewGLSurfaceGLX::GetExtensions() {
  std::string extensions = GLSurface::GetExtensions();
  if (gfx::g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer) {
    extensions += extensions.empty() ? "" : " ";
    extensions += "GL_CHROMIUM_post_sub_buffer";
  }
  return extensions;
}

void* NativeViewGLSurfaceGLX::GetConfig() {
  if (!config_) {
    // This code path is expensive, but we only take it when
    // attempting to use GLX_ARB_create_context_robustness, in which
    // case we need a GLXFBConfig for the window in order to create a
    // context for it.
    //
    // TODO(kbr): this is not a reliable code path. On platforms which
    // support it, we should use glXChooseFBConfig in the browser
    // process to choose the FBConfig and from there the X Visual to
    // use when creating the window in the first place. Then we can
    // pass that FBConfig down rather than attempting to reconstitute
    // it.

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(
        g_display,
        reinterpret_cast<GLXDrawable>(GetHandle()),
        &attributes)) {
      LOG(ERROR) << "XGetWindowAttributes failed for window " <<
          reinterpret_cast<GLXDrawable>(GetHandle()) << ".";
      return NULL;
    }

    int visual_id = XVisualIDFromVisual(attributes.visual);

    int num_elements = 0;
    scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> configs(
        glXGetFBConfigs(g_display,
                        DefaultScreen(g_display),
                        &num_elements));
    if (!configs.get()) {
      LOG(ERROR) << "glXGetFBConfigs failed.";
      return NULL;
    }
    if (!num_elements) {
      LOG(ERROR) << "glXGetFBConfigs returned 0 elements.";
      return NULL;
    }
    bool found = false;
    int i;
    for (i = 0; i < num_elements; ++i) {
      int value;
      if (glXGetFBConfigAttrib(
              g_display, configs.get()[i], GLX_VISUAL_ID, &value)) {
        LOG(ERROR) << "glXGetFBConfigAttrib failed.";
        return NULL;
      }
      if (value == visual_id) {
        found = true;
        break;
      }
    }
    if (found) {
      config_ = configs.get()[i];
    }
  }

  return config_;
}

bool NativeViewGLSurfaceGLX::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(gfx::g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer);
  glXCopySubBufferMESA(g_display, window_, x, y, width, height);
  return true;
}

bool NativeViewGLSurfaceGLX::GetVSyncParameters(base::TimeTicks* timebase,
                                                base::TimeDelta* interval) {
  if (!g_glx_oml_sync_control_supported)
    return false;

  // The actual clock used for the system time returned by glXGetSyncValuesOML
  // is unspecified. In practice, the clock used is likely to be either
  // CLOCK_REALTIME or CLOCK_MONOTONIC, so we compare the returned time to the
  // current time according to both clocks, and assume that the returned time
  // was produced by the clock whose current time is closest to it, subject
  // to the restriction that the returned time must not be in the future (since
  // it is the time of a vblank that has already occurred).
  int64 system_time;
  int64 media_stream_counter;
  int64 swap_buffer_counter;
  if (!glXGetSyncValuesOML(g_display, window_, &system_time,
                           &media_stream_counter, &swap_buffer_counter))
    return false;

  struct timespec real_time;
  struct timespec monotonic_time;
  clock_gettime(CLOCK_REALTIME, &real_time);
  clock_gettime(CLOCK_MONOTONIC, &monotonic_time);

  int64 real_time_in_microseconds =
      real_time.tv_sec * base::Time::kMicrosecondsPerSecond +
      real_time.tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  int64 monotonic_time_in_microseconds =
      monotonic_time.tv_sec * base::Time::kMicrosecondsPerSecond +
      monotonic_time.tv_nsec / base::Time::kNanosecondsPerMicrosecond;

  if ((system_time > real_time_in_microseconds) &&
      (system_time > monotonic_time_in_microseconds))
    return false;

  // We need the time according to CLOCK_MONOTONIC, so if we've been given
  // a time from CLOCK_REALTIME, we need to convert.
  bool time_conversion_needed =
    (system_time > monotonic_time_in_microseconds) ||
    (real_time_in_microseconds - system_time <
     monotonic_time_in_microseconds - system_time);

  if (time_conversion_needed) {
    int64 time_difference =
        real_time_in_microseconds - monotonic_time_in_microseconds;
    *timebase = base::TimeTicks::FromInternalValue(
        system_time - time_difference);
  } else {
    *timebase = base::TimeTicks::FromInternalValue(system_time);
  }

  // On platforms where glXGetMscRateOML doesn't work, we fall back to the
  // assumption that we're displaying 60 frames per second.
  const int64 kDefaultIntervalTime =
      base::Time::kMicrosecondsPerSecond / 60;
  int64 interval_time = kDefaultIntervalTime;
  int32 numerator;
  int32 denominator;
  if (g_glx_get_msc_rate_oml_supported) {
    if (glXGetMscRateOML(g_display, window_, &numerator, &denominator)) {
      interval_time =
          (base::Time::kMicrosecondsPerSecond * denominator) / numerator;
    } else {
      // Once glXGetMscRateOML has been found to fail, don't try again,
      // since each failing call may spew an error message.
      g_glx_get_msc_rate_oml_supported = false;
    }
  }

  *interval = base::TimeDelta::FromMicroseconds(interval_time);
  return true;
}

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX()
  : window_(0),
    config_(NULL) {
}

NativeViewGLSurfaceGLX::~NativeViewGLSurfaceGLX() {
  Destroy();
}

PbufferGLSurfaceGLX::PbufferGLSurfaceGLX(const gfx::Size& size)
  : size_(size),
    config_(NULL),
    pbuffer_(0) {
}

bool PbufferGLSurfaceGLX::Initialize() {
  DCHECK(!pbuffer_);

  static const int config_attributes[] = {
    GLX_BUFFER_SIZE, 32,
    GLX_ALPHA_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_RED_SIZE, 8,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
    GLX_DOUBLEBUFFER, False,
    0
  };

  int num_elements = 0;
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> configs(
      glXChooseFBConfig(g_display,
                        DefaultScreen(g_display),
                        config_attributes,
                        &num_elements));
  if (!configs.get()) {
    LOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!num_elements) {
    LOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }

  config_ = configs.get()[0];

  const int pbuffer_attributes[] = {
    GLX_PBUFFER_WIDTH, size_.width(),
    GLX_PBUFFER_HEIGHT, size_.height(),
    0
  };
  pbuffer_ = glXCreatePbuffer(g_display,
                              static_cast<GLXFBConfig>(config_),
                              pbuffer_attributes);
  if (!pbuffer_) {
    Destroy();
    LOG(ERROR) << "glXCreatePbuffer failed.";
    return false;
  }

  return true;
}

void PbufferGLSurfaceGLX::Destroy() {
  if (pbuffer_) {
    glXDestroyPbuffer(g_display, pbuffer_);
    pbuffer_ = 0;
  }

  config_ = NULL;
}

bool PbufferGLSurfaceGLX::IsOffscreen() {
  return true;
}

bool PbufferGLSurfaceGLX::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
  return false;
}

gfx::Size PbufferGLSurfaceGLX::GetSize() {
  return size_;
}

void* PbufferGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(pbuffer_);
}

void* PbufferGLSurfaceGLX::GetConfig() {
  return config_;
}

PbufferGLSurfaceGLX::~PbufferGLSurfaceGLX() {
  Destroy();
}

}  // namespace gfx
