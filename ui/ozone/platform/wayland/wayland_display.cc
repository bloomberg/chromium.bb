// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_display.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {

WaylandDisplay::WaylandDisplay() {}

WaylandDisplay::~WaylandDisplay() {}

bool WaylandDisplay::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandDisplay::Global, &WaylandDisplay::GlobalRemove,
  };

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "Failed to connect to Wayland display";
    return false;
  }

  registry_.reset(wl_display_get_registry(display_.get()));
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry";
    return false;
  }

  wl_registry_add_listener(registry_.get(), &registry_listener, this);
  wl_display_roundtrip(display_.get());

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!shell_) {
    LOG(ERROR) << "No xdg_shell object";
    return false;
  }

  return true;
}

void WaylandDisplay::Flush() {
  DCHECK(display_);
  wl_display_flush(display_.get());
}

WaylandWindow* WaylandDisplay::GetWindow(gfx::AcceleratedWidget widget) {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

void WaylandDisplay::AddWindow(WaylandWindow* window) {
  window_map_[window->GetWidget()] = window;
}

void WaylandDisplay::RemoveWindow(WaylandWindow* window) {
  window_map_.erase(window->GetWidget());
}

void WaylandDisplay::OnDispatcherListChanged() {
  if (watching_)
    return;

  DCHECK(display_);
  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      wl_display_get_fd(display_.get()), true,
      base::MessagePumpLibevent::WATCH_READ, &controller_, this);
  watching_ = true;
}

void WaylandDisplay::OnFileCanReadWithoutBlocking(int fd) {
  wl_display_dispatch(display_.get());
  for (const auto& window : window_map_)
    window.second->ApplyPendingBounds();
}

void WaylandDisplay::OnFileCanWriteWithoutBlocking(int fd) {}

// static
void WaylandDisplay::Global(void* data,
                            wl_registry* registry,
                            uint32_t name,
                            const char* interface,
                            uint32_t version) {
  static const xdg_shell_listener shell_listener = {
      &WaylandDisplay::Ping,
  };

  WaylandDisplay* display = static_cast<WaylandDisplay*>(data);
  if (!display->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    display->compositor_ = wl::Bind<wl_compositor>(registry, name, version);
    if (!display->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!display->shm_ && strcmp(interface, "wl_shm") == 0) {
    display->shm_ = wl::Bind<wl_shm>(registry, name, version);
    if (!display->shm_)
      LOG(ERROR) << "Failed to bind to wl_shm global";
  } else if (!display->shell_ && strcmp(interface, "xdg_shell") == 0) {
    display->shell_ = wl::Bind<xdg_shell>(registry, name, version);
    if (!display->shell_) {
      LOG(ERROR) << "Failed to  bind to xdg_shell global";
      return;
    }
    xdg_shell_add_listener(display->shell_.get(), &shell_listener, display);
    xdg_shell_use_unstable_version(display->shell_.get(),
                                   XDG_SHELL_VERSION_CURRENT);
  }
}

// static
void WaylandDisplay::GlobalRemove(void* data,
                                  wl_registry* registry,
                                  uint32_t name) {
  NOTIMPLEMENTED();
}

// static
void WaylandDisplay::Ping(void* data, xdg_shell* shell, uint32_t serial) {
  xdg_shell_pong(shell, serial);
}

}  // namespace ui
