// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DISPLAY_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DISPLAY_H_

#include <map>

#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

namespace ui {

class WaylandWindow;

class OZONE_EXPORT WaylandDisplay : public PlatformEventSource,
                                    public base::MessagePumpLibevent::Watcher {
 public:
  WaylandDisplay();
  ~WaylandDisplay() override;

  bool Initialize();

  // Flushes the Wayland connection.
  void Flush();

  wl_display* display() { return display_.get(); }
  wl_compositor* compositor() { return compositor_.get(); }
  wl_shm* shm() { return shm_.get(); }
  xdg_shell* shell() { return shell_.get(); }

  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget);
  void AddWindow(WaylandWindow* window);
  void RemoveWindow(WaylandWindow* window);

 private:
  // PlatformEventSource
  void OnDispatcherListChanged() override;

  // base::MessagePumpLibevent::Watcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // wl_registry_listener
  static void Global(void* data,
                     wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version);
  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // xdg_shell_listener
  static void Ping(void* data, xdg_shell* shell, uint32_t serial);

  std::map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  wl::Object<wl_display> display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_shm> shm_;
  wl::Object<xdg_shell> shell_;

  bool watching_ = false;
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplay);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DISPLAY_H_
