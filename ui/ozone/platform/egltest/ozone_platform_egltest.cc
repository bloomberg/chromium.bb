// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/egltest/ozone_platform_egltest.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "library_loaders/libeglplatform_shim.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"
#endif

#include <EGL/egl.h>

namespace ui {

namespace {

const char kEglplatformShim[] = "EGLPLATFORM_SHIM";
const char kEglplatformShimDefault[] = "libeglplatform_shim.so.1";
const char kDefaultEglSoname[] = "libEGL.so.1";
const char kDefaultGlesSoname[] = "libGLESv2.so.2";

// Get the library soname to load.
std::string GetShimLibraryName() {
  std::string library;
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(kEglplatformShim, &library))
    return library;
  return kEglplatformShimDefault;
}

class EgltestWindow : public PlatformWindow, public PlatformEventDispatcher {
 public:
  EgltestWindow(PlatformWindowDelegate* delegate,
                LibeglplatformShimLoader* eglplatform_shim,
                const gfx::Rect& bounds);
  virtual ~EgltestWindow();

  // PlatformWindow:
  virtual gfx::Rect GetBounds() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void ToggleFullscreen() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;

  // PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const PlatformEvent& event) OVERRIDE;

 private:
  PlatformWindowDelegate* delegate_;
  LibeglplatformShimLoader* eglplatform_shim_;
  gfx::Rect bounds_;
  ShimNativeWindowId window_id_;

  DISALLOW_COPY_AND_ASSIGN(EgltestWindow);
};

EgltestWindow::EgltestWindow(PlatformWindowDelegate* delegate,
                             LibeglplatformShimLoader* eglplatform_shim,
                             const gfx::Rect& bounds)
    : delegate_(delegate),
      eglplatform_shim_(eglplatform_shim),
      bounds_(bounds),
      window_id_(SHIM_NO_WINDOW_ID) {
  window_id_ = eglplatform_shim_->ShimCreateWindow();
  delegate_->OnAcceleratedWidgetAvailable(window_id_);
  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

EgltestWindow::~EgltestWindow() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  if (window_id_ != SHIM_NO_WINDOW_ID)
    eglplatform_shim_->ShimDestroyWindow(window_id_);
}

gfx::Rect EgltestWindow::GetBounds() {
  return bounds_;
}

void EgltestWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void EgltestWindow::Show() {
}

void EgltestWindow::Hide() {
}

void EgltestWindow::Close() {
}

void EgltestWindow::SetCapture() {
}

void EgltestWindow::ReleaseCapture() {
}

void EgltestWindow::ToggleFullscreen() {
}

void EgltestWindow::Maximize() {
}

void EgltestWindow::Minimize() {
}

void EgltestWindow::Restore() {
}

bool EgltestWindow::CanDispatchEvent(const ui::PlatformEvent& ne) {
  return true;
}

uint32_t EgltestWindow::DispatchEvent(const ui::PlatformEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  delegate_->DispatchEvent(event);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

// EGL surface wrapper for libeglplatform_shim.
//
// This just manages the native window lifetime using
// ShimGetNativeWindow & ShimReleaseNativeWindow.
class SurfaceOzoneEgltest : public SurfaceOzoneEGL {
 public:
  SurfaceOzoneEgltest(ShimNativeWindowId window_id,
                      LibeglplatformShimLoader* eglplatform_shim)
      : eglplatform_shim_(eglplatform_shim) {
    native_window_ = eglplatform_shim_->ShimGetNativeWindow(window_id);
  }
  virtual ~SurfaceOzoneEgltest() {
    CHECK(eglplatform_shim_->ShimReleaseNativeWindow(native_window_));
  }

  virtual intptr_t GetNativeWindow() OVERRIDE { return native_window_; }

  virtual bool OnSwapBuffers() OVERRIDE { return true; }

  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) OVERRIDE {
    return true;
  }

  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE {
    return scoped_ptr<gfx::VSyncProvider>();
  }

  virtual bool ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    scoped_refptr<ui::NativePixmap> buffer,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) OVERRIDE {
    return false;
  }

 private:
  LibeglplatformShimLoader* eglplatform_shim_;
  intptr_t native_window_;
};

// EGL surface factory for libeglplatform_shim.
//
// This finds the right EGL/GLES2 libraries for loading, and creates
// a single native window via ShimCreateWindow for drawing
// into.
class SurfaceFactoryEgltest : public ui::SurfaceFactoryOzone {
 public:
  SurfaceFactoryEgltest(LibeglplatformShimLoader* eglplatform_shim)
      : eglplatform_shim_(eglplatform_shim) {}
  virtual ~SurfaceFactoryEgltest() {}

  // SurfaceFactoryOzone:
  virtual intptr_t GetNativeDisplay() OVERRIDE;
  virtual scoped_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual const int32* GetEGLSurfaceProperties(
      const int32* desired_list) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

 private:
  LibeglplatformShimLoader* eglplatform_shim_;
};

intptr_t SurfaceFactoryEgltest::GetNativeDisplay() {
  return eglplatform_shim_->ShimGetNativeDisplay();
}

scoped_ptr<SurfaceOzoneEGL> SurfaceFactoryEgltest::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return make_scoped_ptr<SurfaceOzoneEGL>(
      new SurfaceOzoneEgltest(widget, eglplatform_shim_));
}

bool SurfaceFactoryEgltest::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  const char* egl_soname = eglplatform_shim_->ShimQueryString(SHIM_EGL_LIBRARY);
  const char* gles_soname =
      eglplatform_shim_->ShimQueryString(SHIM_GLES_LIBRARY);
  if (!egl_soname)
    egl_soname = kDefaultEglSoname;
  if (!gles_soname)
    gles_soname = kDefaultGlesSoname;

  base::NativeLibraryLoadError error;
  base::NativeLibrary egl_library =
      base::LoadNativeLibrary(base::FilePath(egl_soname), &error);
  if (!egl_library) {
    LOG(WARNING) << "Failed to load EGL library: " << error.ToString();
    return false;
  }

  base::NativeLibrary gles_library =
      base::LoadNativeLibrary(base::FilePath(gles_soname), &error);
  if (!gles_library) {
    LOG(WARNING) << "Failed to load GLES library: " << error.ToString();
    base::UnloadNativeLibrary(egl_library);
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                    "eglGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  set_gl_get_proc_address.Run(get_proc_address);
  add_gl_library.Run(egl_library);
  add_gl_library.Run(gles_library);
  return true;
}

const int32* SurfaceFactoryEgltest::GetEGLSurfaceProperties(
    const int32* desired_list) {
  static const int32 broken_props[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
      EGL_NONE,
  };
  return broken_props;
}

// Test platform for EGL.
//
// This is a tiny EGL-based platform. Creation of the native window is
// handled by a separate library called eglplatform_shim.so.1 because
// this itself is platform specific and we want to test out multiple
// hardware platforms.
class OzonePlatformEgltest : public OzonePlatform {
 public:
  OzonePlatformEgltest() : shim_initialized_(false) {}
  virtual ~OzonePlatformEgltest() {
    if (shim_initialized_)
      eglplatform_shim_.ShimTerminate();
  }

  void LoadShim() {
    std::string library = GetShimLibraryName();

    if (eglplatform_shim_.Load(library))
      return;

    base::FilePath module_path;
    if (!PathService::Get(base::DIR_MODULE, &module_path))
      LOG(ERROR) << "failed to get DIR_MODULE from PathService";
    base::FilePath library_path = module_path.Append(library);

    if (eglplatform_shim_.Load(library_path.value()))
      return;

    LOG(FATAL) << "failed to load " << library;
  }

  void Initialize() {
    LoadShim();
    shim_initialized_ = eglplatform_shim_.ShimInitialize();
  }

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return event_factory_ozone_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE {
    return gpu_platform_support_.get();
  }
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE {
    return gpu_platform_support_host_.get();
  }
  virtual scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) OVERRIDE {
    return make_scoped_ptr<PlatformWindow>(
        new EgltestWindow(delegate, &eglplatform_shim_, bounds));
  }

#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
  virtual scoped_ptr<TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() OVERRIDE {
    return scoped_ptr<TouchscreenDeviceManager>(
        new TouchscreenDeviceManagerOzone());
  }
#endif

  virtual void InitializeUI() OVERRIDE {
    device_manager_ = CreateDeviceManager();
    if (!surface_factory_ozone_)
      surface_factory_ozone_.reset(
          new SurfaceFactoryEgltest(&eglplatform_shim_));
    event_factory_ozone_.reset(
        new EventFactoryEvdev(NULL, device_manager_.get()));
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  virtual void InitializeGPU() OVERRIDE {
    if (!surface_factory_ozone_)
      surface_factory_ozone_.reset(
          new SurfaceFactoryEgltest(&eglplatform_shim_));
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  LibeglplatformShimLoader eglplatform_shim_;
  scoped_ptr<DeviceManager> device_manager_;
  scoped_ptr<SurfaceFactoryEgltest> surface_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  bool shim_initialized_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformEgltest);
};

}  // namespace

OzonePlatform* CreateOzonePlatformEgltest() {
  OzonePlatformEgltest* platform = new OzonePlatformEgltest;
  platform->Initialize();
  return platform;
}

}  // namespace ui
