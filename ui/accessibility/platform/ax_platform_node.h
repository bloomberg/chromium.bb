// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/native_widget_types.h"

// Set PLATFORM_HAS_AX_PLATFORM_NODE_IMPL if this platform has a specific
// implementation of AxPlatfromNode::Create().
#undef PLATFORM_HAS_AX_PLATFORM_NODE_IMPL

#if defined(OS_WIN)
#define PLATFORM_HAS_AX_PLATFORM_NODE_IMPL 1
#endif

#if defined(OS_MACOSX)
#define PLATFORM_HAS_AX_PLATFORM_NODE_IMPL 1
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_X11)
#define PLATFORM_HAS_AX_PLATFORM_NODE_IMPL 1
#endif

namespace ui {

class AXPlatformNodeDelegate;

// AXPlatformNode is the abstract base class for an implementation of
// native accessibility APIs on supported platforms (e.g. Windows, Mac OS X).
// An object that wants to be accessible can derive from AXPlatformNodeDelegate
// and then call AXPlatformNode::Create. The delegate implementation should
// own the AXPlatformNode instance (or otherwise manage its lifecycle).
class AX_EXPORT AXPlatformNode {
 public:
  // Create an appropriate platform-specific instance. The delegate owns the
  // AXPlatformNode instance (or manages its lifecycle in some other way).
  static AXPlatformNode* Create(AXPlatformNodeDelegate* delegate);

  // Cast a gfx::NativeViewAccessible to an AXPlatformNode if it is one,
  // or return NULL if it's not an instance of this class.
  static AXPlatformNode* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  virtual void Destroy() = 0;

  // Get the platform-specific accessible object type for this instance.
  // On some platforms this is just a type cast, on others it may be a
  // wrapper object or handle.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Fire a platform-specific notification that an event has occurred on
  // this object.
  virtual void NotifyAccessibilityEvent(ui::AXEvent event_type) = 0;

  // Return this object's delegate.
  virtual AXPlatformNodeDelegate* GetDelegate() const = 0;

 protected:
  AXPlatformNode();
  virtual ~AXPlatformNode();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_
