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
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
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
bool g_glx_context_create = false;
bool g_glx_create_context_robustness_supported = false;
bool g_glx_texture_from_pixmap_supported = false;
bool g_glx_oml_sync_control_supported = false;

// Track support of glXGetMscRateOML separately from GLX_OML_sync_control as a
// whole since on some platforms (e.g. crosbug.com/34585), glXGetMscRateOML
// always fails even though GLX_OML_sync_control is reported as being supported.
bool g_glx_get_msc_rate_oml_supported = false;

bool g_glx_sgi_video_sync_supported = false;

class OMLSyncControlVSyncProvider
    : public gfx::NativeViewGLSurfaceGLX::VSyncProvider {
 public:
  explicit OMLSyncControlVSyncProvider(gfx::AcceleratedWidget window)
      : window_(window),
        last_media_stream_counter_(0) {
    // On platforms where we can't get an accurate reading on the refresh
    // rate we fall back to the assumption that we're displaying 60 frames
    // per second.
    last_good_interval_ = base::TimeDelta::FromSeconds(1) / 60;
  }

  virtual ~OMLSyncControlVSyncProvider() { }

  virtual void GetVSyncParameters(
      const GLSurface::UpdateVSyncCallback& callback) OVERRIDE {
    base::TimeTicks timebase;

    // The actual clock used for the system time returned by glXGetSyncValuesOML
    // is unspecified. In practice, the clock used is likely to be either
    // CLOCK_REALTIME or CLOCK_MONOTONIC, so we compare the returned time to the
    // current time according to both clocks, and assume that the returned time
    // was produced by the clock whose current time is closest to it, subject
    // to the restriction that the returned time must not be in the future
    // (since it is the time of a vblank that has already occurred).
    int64 system_time;
    int64 media_stream_counter;
    int64 swap_buffer_counter;
    if (!glXGetSyncValuesOML(g_display, window_, &system_time,
                             &media_stream_counter, &swap_buffer_counter))
      return;

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
      return;

    // We need the time according to CLOCK_MONOTONIC, so if we've been given
    // a time from CLOCK_REALTIME, we need to convert.
    bool time_conversion_needed =
      (system_time > monotonic_time_in_microseconds) ||
      (real_time_in_microseconds - system_time <
       monotonic_time_in_microseconds - system_time);

    if (time_conversion_needed) {
      int64 time_difference =
        real_time_in_microseconds - monotonic_time_in_microseconds;
      timebase = base::TimeTicks::FromInternalValue(
          system_time - time_difference);
    } else {
      timebase = base::TimeTicks::FromInternalValue(system_time);
    }

    if (g_glx_get_msc_rate_oml_supported) {
      int32 numerator, denominator;
      if (glXGetMscRateOML(g_display, window_, &numerator, &denominator)) {
        last_good_interval_ =
            base::TimeDelta::FromSeconds(denominator) / numerator;
      } else {
        // Once glXGetMscRateOML has been found to fail, don't try again,
        // since each failing call may spew an error message.
        g_glx_get_msc_rate_oml_supported = false;
      }
    } else {
      if (!last_timebase_.is_null()) {
        base::TimeDelta timebase_diff = timebase - last_timebase_;
        uint64 counter_diff = media_stream_counter -
            last_media_stream_counter_;
        if (counter_diff > 0 && timebase > last_timebase_)
          last_good_interval_ = timebase_diff / counter_diff;
      }
    }
    last_timebase_ = timebase;
    last_media_stream_counter_ = media_stream_counter;
    callback.Run(timebase, last_good_interval_);
  }

 private:
  XID window_;

  base::TimeTicks last_timebase_;
  uint64 last_media_stream_counter_;
  base::TimeDelta last_good_interval_;

  DISALLOW_COPY_AND_ASSIGN(OMLSyncControlVSyncProvider);
};

class SGIVideoSyncThread
     : public base::Thread,
       public base::NonThreadSafe,
       public base::RefCounted<SGIVideoSyncThread> {
 public:
  static scoped_refptr<SGIVideoSyncThread> Create() {
    if (!g_video_sync_thread) {
      g_video_sync_thread = new SGIVideoSyncThread();
      g_video_sync_thread->Start();
    }
    return g_video_sync_thread;
  }

 private:
  friend class base::RefCounted<SGIVideoSyncThread>;

  SGIVideoSyncThread() : base::Thread("SGI_video_sync") {
    DCHECK(CalledOnValidThread());
  }

  ~SGIVideoSyncThread() {
    DCHECK(CalledOnValidThread());
    g_video_sync_thread = NULL;
    Stop();
  }

  static SGIVideoSyncThread* g_video_sync_thread;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncThread);
};

class SGIVideoSyncProviderThreadShim
    : public base::SupportsWeakPtr<SGIVideoSyncProviderThreadShim> {
 public:
  explicit SGIVideoSyncProviderThreadShim(XID window)
      : window_(window),
        context_(NULL),
        message_loop_(base::MessageLoopProxy::current()),
        cancel_vsync_flag_(),
        vsync_lock_() {
    // This ensures that creation of |window_| has occured when this shim
    // is executing in the same process as the call to create |window_|.
    XSync(g_display, False);
  }

  base::CancellationFlag* cancel_vsync_flag() {
    return &cancel_vsync_flag_;
  }

  base::Lock* vsync_lock() {
    return &vsync_lock_;
  }

  void Initialize() {
    DCHECK(display_);

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display_, window_, &attributes)) {
      LOG(ERROR) << "XGetWindowAttributes failed for window " <<
        window_ << ".";
      return;
    }

    XVisualInfo visual_info_template;
    visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);

    int visual_info_count = 0;
    scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info_list(
        XGetVisualInfo(display_, VisualIDMask,
                       &visual_info_template, &visual_info_count));

    DCHECK(visual_info_list.get());
    if (visual_info_count == 0) {
      LOG(ERROR) << "No visual info for visual ID.";
      return;
    }

    context_ = glXCreateContext(display_, visual_info_list.get(), NULL, True);

    DCHECK(NULL != context_);
  }

  void Destroy() {
    if (context_) {
      glXDestroyContext(display_, context_);
      context_ = NULL;
    }
    delete this;
  }

  void GetVSyncParameters(const GLSurface::UpdateVSyncCallback& callback) {
    base::TimeTicks now;
    {
      // Don't allow |window_| destruction while we're probing vsync.
      base::AutoLock locked(vsync_lock_);

      if (!context_ || cancel_vsync_flag_.IsSet())
        return;

      glXMakeCurrent(display_, window_, context_);

      unsigned int retrace_count = 0;
      if (glXWaitVideoSyncSGI(1, 0, &retrace_count) != 0)
        return;

      TRACE_EVENT_INSTANT0("gpu", "vblank");
      now = base::TimeTicks::HighResNow();

      glXMakeCurrent(display_, 0, 0);
    }

    const base::TimeDelta kDefaultInterval =
        base::TimeDelta::FromSeconds(1) / 60;

    message_loop_->PostTask(
        FROM_HERE, base::Bind(callback, now, kDefaultInterval));
  }

 private:
  // For initialization of display_ in GLSurface::InitializeOneOff before
  // the sandbox goes up.
  friend class gfx::GLSurfaceGLX;

  virtual ~SGIVideoSyncProviderThreadShim() {
  }

  static Display* display_;

  XID window_;
  GLXContext context_;

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  base::CancellationFlag cancel_vsync_flag_;
  base::Lock vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncProviderThreadShim);
};

class SGIVideoSyncVSyncProvider
    : public gfx::NativeViewGLSurfaceGLX::VSyncProvider,
      public base::SupportsWeakPtr<SGIVideoSyncVSyncProvider> {
 public:
  explicit SGIVideoSyncVSyncProvider(gfx::AcceleratedWidget window)
      : vsync_thread_(SGIVideoSyncThread::Create()),
        shim_((new SGIVideoSyncProviderThreadShim(window))->AsWeakPtr()),
        cancel_vsync_flag_(shim_->cancel_vsync_flag()),
        vsync_lock_(shim_->vsync_lock()) {
    // The WeakPtr is bound to the SGIVideoSyncThread. We only use it for
    // PostTask.
    shim_->DetachFromThread();
    vsync_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SGIVideoSyncProviderThreadShim::Initialize, shim_));
  }

  virtual ~SGIVideoSyncVSyncProvider() {
    {
      base::AutoLock locked(*vsync_lock_);
      cancel_vsync_flag_->Set();
    }
    vsync_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SGIVideoSyncProviderThreadShim::Destroy, shim_));
  }

  virtual void GetVSyncParameters(
      const GLSurface::UpdateVSyncCallback& callback) OVERRIDE {
    // Only one outstanding request per surface.
    if (!pending_callback_) {
      pending_callback_.reset(new GLSurface::UpdateVSyncCallback(callback));
      vsync_thread_->message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SGIVideoSyncProviderThreadShim::GetVSyncParameters,
                     shim_, base::Bind(
                         &SGIVideoSyncVSyncProvider::PendingCallbackRunner,
                         AsWeakPtr())));
    }
  }

 private:
  void PendingCallbackRunner(const base::TimeTicks timebase,
                             const base::TimeDelta interval) {
    DCHECK(pending_callback_);
    pending_callback_->Run(timebase, interval);
    pending_callback_.reset();
  }

  scoped_refptr<SGIVideoSyncThread> vsync_thread_;
  base::WeakPtr<SGIVideoSyncProviderThreadShim> shim_;

  scoped_ptr<GLSurface::UpdateVSyncCallback> pending_callback_;

  // Raw pointers to sync primitives owned by the shim_.
  // These will only be referenced before we post a task to destroy
  // the shim_, so they are safe to access.
  base::CancellationFlag* cancel_vsync_flag_;
  base::Lock* vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncVSyncProvider);
};

SGIVideoSyncThread* SGIVideoSyncThread::g_video_sync_thread = NULL;

// In order to take advantage of GLX_SGI_video_sync, we need a display
// for use on a separate thread. We must allocate this before the sandbox
// goes up (rather than on-demand when we start the thread).
Display* SGIVideoSyncProviderThreadShim::display_ = NULL;

}  // namespace

GLSurfaceGLX::GLSurfaceGLX() {}

bool GLSurfaceGLX::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  // SGIVideoSyncProviderShim (if instantiated) will issue X commands on
  // it's own thread.
  XInitThreads();

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
  g_glx_context_create =
      HasGLXExtension("GLX_ARB_create_context");
  g_glx_create_context_robustness_supported =
      HasGLXExtension("GLX_ARB_create_context_robustness");
  g_glx_texture_from_pixmap_supported =
      HasGLXExtension("GLX_EXT_texture_from_pixmap");
  g_glx_oml_sync_control_supported =
      HasGLXExtension("GLX_OML_sync_control");
  g_glx_get_msc_rate_oml_supported = g_glx_oml_sync_control_supported;
  g_glx_sgi_video_sync_supported =
      HasGLXExtension("GLX_SGI_video_sync");

  if (!g_glx_get_msc_rate_oml_supported && g_glx_sgi_video_sync_supported)
    SGIVideoSyncProviderThreadShim::display_ = XOpenDisplay(NULL);

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
bool GLSurfaceGLX::IsCreateContextSupported() {
  return g_glx_context_create;
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

  if (g_glx_oml_sync_control_supported)
    vsync_provider_.reset(new OMLSyncControlVSyncProvider(window_));
  else if (g_glx_sgi_video_sync_supported)
    vsync_provider_.reset(new SGIVideoSyncVSyncProvider(window_));

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
        window_,
        &attributes)) {
      LOG(ERROR) << "XGetWindowAttributes failed for window " <<
          window_ << ".";
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

void NativeViewGLSurfaceGLX::GetVSyncParameters(
    const UpdateVSyncCallback& callback) {
  if (vsync_provider_)
    vsync_provider_->GetVSyncParameters(callback);
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
