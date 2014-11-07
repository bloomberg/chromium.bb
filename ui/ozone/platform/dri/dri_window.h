// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/channel_observer.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class DriWindowDelegate;
class DriWindowManager;
class EventFactoryEvdev;
class DriGpuPlatformSupportHost;

class DriWindow : public PlatformWindow,
                  public PlatformEventDispatcher,
                  public ChannelObserver {
 public:
  DriWindow(PlatformWindowDelegate* delegate,
            const gfx::Rect& bounds,
            DriGpuPlatformSupportHost* sender,
            EventFactoryEvdev* event_factory,
            DriWindowManager* window_manager);
  ~DriWindow() override;

  void Initialize();

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // ChannelObserver:
  void OnChannelEstablished() override;
  void OnChannelDestroyed() override;

 private:
  PlatformWindowDelegate* delegate_;   // Not owned.
  DriGpuPlatformSupportHost* sender_;  // Not owned.
  EventFactoryEvdev* event_factory_;   // Not owned.
  DriWindowManager* window_manager_;   // Not owned.

  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(DriWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_H_
