// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_SURFACE_LNUX_FACTORY_OZONE_H_
#define UI_GFX_OZONE_SURFACE_LNUX_FACTORY_OZONE_H_

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Screen;
class VSyncProvider;

class GFX_EXPORT SurfaceFactoryOzone {
 public:
  // Describes the state of the hardware after initialization.
  enum HardwareState {
    INITIALIZED,
    FAILED,
  };

  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();

  // Returns the instance
  static SurfaceFactoryOzone* GetInstance();

  // Returns a display spec as in |CreateDisplayFromSpec| for the default
  // native surface.
  virtual const char* DefaultDisplaySpec();

  // Sets the implementation delegate. Ownership is retained by the caller.
  static void SetInstance(SurfaceFactoryOzone* impl);

  // TODO(rjkroege): decide how to separate screen/display stuff from SFOz
  // This method implements gfx::Screen, particularly useful in Desktop Aura.
  virtual gfx::Screen* CreateDesktopScreen();

  // Configures the display hardware. Must be called from within the GPU
  // process before the sandbox has been activated.
  virtual HardwareState InitializeHardware() = 0;

  // Cleans up display hardware state. Call this from within the GPU process.
  // This method must be safe to run inside of the sandbox.
  virtual void ShutdownHardware() = 0;

  // Returns the native EGL display. This is generally needed in creating
  // EGL windows.
  virtual intptr_t GetNativeDisplay();

  // Obtains an AcceleratedWidget backed by a native Linux framebuffer.
  // The  returned AcceleratedWidget is an opaque token that must realized
  // before it can be used to create a GL surface.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Realizes an AcceleratedWidget so that the returned AcceleratedWidget
  // can be used to to create a GLSurface. This method may only be called in
  // a process that has a valid GL context.
  virtual gfx::AcceleratedWidget RealizeAcceleratedWidget(
      gfx::AcceleratedWidget w) = 0;

  // Sets up GL bindings for the native surface.
  virtual bool LoadEGLGLES2Bindings() = 0;

  // If possible attempts to resize the given AcceleratedWidget instance and if
  // a resize action was performed returns true, otherwise false (native
  // hardware may only support a single fixed size).
  virtual bool AttemptToResizeAcceleratedWidget(
      gfx::AcceleratedWidget w,
      const gfx::Rect& bounds) = 0;

  // Called after the appropriate GL swap buffers command. Used if extra work
  // is needed to perform the actual buffer swap.
  virtual bool SchedulePageFlip(gfx::AcceleratedWidget w);

  // Returns a gfx::VsyncProvider for the provided AcceleratedWidget. Note
  // that this may be called after we have entered the sandbox so if there are
  // operations (e.g. opening a file descriptor providing vsync events) that
  // must be done outside of the sandbox, they must have been completed
  // in InitializeHardware. Returns NULL on error.
  virtual gfx::VSyncProvider* GetVSyncProvider(gfx::AcceleratedWidget w) = 0;

  // Returns an array of EGL properties, which can be used in any EGL function
  // used to select a display configuration. Note that all properties should be
  // immediately followed by the corresponding desired value and array should be
  // terminated with EGL_NONE. Ownership of the array is not transferred to
  // caller. desired_list contains list of desired EGL properties and values.
  virtual const int32* GetEGLSurfaceProperties(const int32* desired_list);

  // Create a default SufaceFactoryOzone implementation useful for tests.
  static SurfaceFactoryOzone* CreateTestHelper();

 private:
  static SurfaceFactoryOzone* impl_; // not owned
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_SURFACE_LNUX_FACTORY_OZONE_H_
