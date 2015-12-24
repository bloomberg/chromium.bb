// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
#define UI_OZONE_PUBLIC_OZONE_PLATFORM_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/ozone/ozone_export.h"

namespace gfx {
class Rect;
}

namespace ui {

class CursorFactoryOzone;
class InputController;
class GpuPlatformSupport;
class GpuPlatformSupportHost;
class NativeDisplayDelegate;
class OverlayManagerOzone;
class PlatformWindow;
class PlatformWindowDelegate;
class SurfaceFactoryOzone;
class SystemInputInjector;

// Base class for Ozone platform implementations.
//
// Ozone platforms must override this class and implement the virtual
// GetFooFactoryOzone() methods to provide implementations of the
// various ozone interfaces.
//
// The OzonePlatform subclass can own any state needed by the
// implementation that is shared between the various ozone interfaces,
// such as a connection to the windowing system.
//
// A platform is free to use different implementations of each
// interface depending on the context. You can, for example, create
// different objects depending on the underlying hardware, command
// line flags, or whatever is appropriate for the platform.
class OZONE_EXPORT OzonePlatform {
 public:
  OzonePlatform();
  virtual ~OzonePlatform();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events, surface, etc.)
  static void InitializeForUI();

  // Initializes the subsystems/resources necessary for the GPU process.
  static void InitializeForGPU();

  static OzonePlatform* GetInstance();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::OverlayManagerOzone* GetOverlayManager() = 0;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() = 0;
  virtual ui::InputController* GetInputController() = 0;
  virtual ui::GpuPlatformSupport* GetGpuPlatformSupport() = 0;
  virtual ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
  virtual scoped_ptr<SystemInputInjector> CreateSystemInputInjector() = 0;
  virtual scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) = 0;
  virtual scoped_ptr<ui::NativeDisplayDelegate>
      CreateNativeDisplayDelegate() = 0;
  // Open ClientNativePixmap device file for non-GPU processes to import a
  // ClientNativePixmap.
  virtual base::ScopedFD OpenClientNativePixmapDevice() const = 0;

 private:
  virtual void InitializeUI() = 0;
  virtual void InitializeGPU() = 0;

  static void CreateInstance();

  static OzonePlatform* instance_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
