// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/fake_server.h"

#include <sys/socket.h>
#include <wayland-server.h>
#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"

namespace wl {
namespace {

const uint32_t kCompositorVersion = 4;
const uint32_t kOutputVersion = 2;
const uint32_t kSeatVersion = 4;
const uint32_t kXdgShellVersion = 1;

void DestroyResource(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

// wl_compositor

void CreateSurface(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* compositor =
      static_cast<MockCompositor*>(wl_resource_get_user_data(resource));
  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  if (!surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  compositor->AddSurface(std::make_unique<MockSurface>(surface_resource));
}

const struct wl_compositor_interface compositor_impl = {
    &CreateSurface,  // create_surface
    nullptr,         // create_region
};

// wl_surface

void Attach(wl_client* client,
            wl_resource* resource,
            wl_resource* buffer_resource,
            int32_t x,
            int32_t y) {
  static_cast<MockSurface*>(wl_resource_get_user_data(resource))
      ->Attach(buffer_resource, x, y);
}

void Damage(wl_client* client,
            wl_resource* resource,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height) {
  static_cast<MockSurface*>(wl_resource_get_user_data(resource))
      ->Damage(x, y, width, height);
}

void Commit(wl_client* client, wl_resource* resource) {
  static_cast<MockSurface*>(wl_resource_get_user_data(resource))->Commit();
}

const struct wl_surface_interface surface_impl = {
    &DestroyResource,  // destroy
    &Attach,           // attach
    &Damage,           // damage
    nullptr,           // frame
    nullptr,           // set_opaque_region
    nullptr,           // set_input_region
    &Commit,           // commit
    nullptr,           // set_buffer_transform
    nullptr,           // set_buffer_scale
    nullptr,           // damage_buffer
};

// xdg_shell

void UseUnstableVersion(wl_client* client,
                        wl_resource* resource,
                        int32_t version) {
  static_cast<MockXdgShell*>(wl_resource_get_user_data(resource))
      ->UseUnstableVersion(version);
}

// xdg_shell and zxdg_shell_v6

void Pong(wl_client* client, wl_resource* resource, uint32_t serial) {
  static_cast<MockXdgShell*>(wl_resource_get_user_data(resource))->Pong(serial);
}

// xdg_shell

void GetXdgSurfaceV5(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource);

const struct xdg_shell_interface xdg_shell_impl = {
    &DestroyResource,     // destroy
    &UseUnstableVersion,  // use_unstable_version
    &GetXdgSurfaceV5,     // get_xdg_surface
    nullptr,              // get_xdg_popup
    &Pong,                // pong
};

// zxdg_shell_v6

void GetXdgSurfaceV6(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource);

const struct zxdg_shell_v6_interface zxdg_shell_v6_impl = {
    &DestroyResource,  // destroy
    nullptr,           // create_positioner
    &GetXdgSurfaceV6,  // get_xdg_surface
    &Pong,             // pong
};

// wl_seat

void GetPointer(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* seat = static_cast<MockSeat*>(wl_resource_get_user_data(resource));
  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  if (!pointer_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  seat->pointer.reset(new MockPointer(pointer_resource));
}

void GetKeyboard(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* seat = static_cast<MockSeat*>(wl_resource_get_user_data(resource));
  wl_resource* keyboard_resource = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
  if (!keyboard_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  seat->keyboard.reset(new MockKeyboard(keyboard_resource));
}

const struct wl_seat_interface seat_impl = {
    &GetPointer,       // get_pointer
    &GetKeyboard,      // get_keyboard
    nullptr,           // get_touch,
    &DestroyResource,  // release
};

// wl_keyboard

const struct wl_keyboard_interface keyboard_impl = {
    &DestroyResource,  // release
};

// wl_pointer

const struct wl_pointer_interface pointer_impl = {
    nullptr,           // set_cursor
    &DestroyResource,  // release
};

// xdg_surface, zxdg_surface_v6 and zxdg_toplevel shared methods.

void SetTitle(wl_client* client, wl_resource* resource, const char* title) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->SetTitle(title);
}

void SetAppId(wl_client* client, wl_resource* resource, const char* app_id) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->SetAppId(app_id);
}

void AckConfigure(wl_client* client, wl_resource* resource, uint32_t serial) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->AckConfigure(serial);
}

void SetWindowGeometry(wl_client* client,
                       wl_resource* resource,
                       int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->SetWindowGeometry(x, y, width, height);
}

void SetMaximized(wl_client* client, wl_resource* resource) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->SetMaximized();
}

void UnsetMaximized(wl_client* client, wl_resource* resource) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->UnsetMaximized();
}

void SetMinimized(wl_client* client, wl_resource* resource) {
  static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource))
      ->SetMinimized();
}

const struct xdg_surface_interface xdg_surface_impl = {
    &DestroyResource,    // destroy
    nullptr,             // set_parent
    &SetTitle,           // set_title
    &SetAppId,           // set_app_id
    nullptr,             // show_window_menu
    nullptr,             // move
    nullptr,             // resize
    &AckConfigure,       // ack_configure
    &SetWindowGeometry,  // set_window_geometry
    &SetMaximized,       // set_maximized
    &UnsetMaximized,     // set_unmaximized
    nullptr,             // set_fullscreen
    nullptr,             // unset_fullscreen
    &SetMinimized,       // set_minimized
};

// zxdg_surface specific interface

void GetTopLevel(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* surface =
      static_cast<MockXdgSurface*>(wl_resource_get_user_data(resource));
  if (surface->xdg_toplevel) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }
  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);
  if (!xdg_toplevel_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->xdg_toplevel.reset(new MockXdgTopLevel(xdg_toplevel_resource));
}

const struct zxdg_surface_v6_interface zxdg_surface_v6_impl = {
    &DestroyResource,    // destroy
    &GetTopLevel,        // get_toplevel
    nullptr,             // get_popup
    &SetWindowGeometry,  // set_window_geometry
    &AckConfigure,       // ack_configure
};

const struct zxdg_toplevel_v6_interface zxdg_toplevel_v6_impl = {
    &DestroyResource,  // destroy
    nullptr,           // set_parent
    &SetTitle,         // set_title
    &SetAppId,         // set_app_id
    nullptr,           // show_window_menu
    nullptr,           // move
    nullptr,           // resize
    nullptr,           // set_max_size
    nullptr,           // set_min_size
    &SetMaximized,     // set_maximized
    &UnsetMaximized,   // set_unmaximized
    nullptr,           // set_fullscreen
    nullptr,           // unset_fullscreen
    &SetMinimized,     // set_minimized
};

void GetXdgSurfaceImpl(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       wl_resource* surface_resource,
                       const struct wl_interface* interface,
                       const void* implementation) {
  auto* surface =
      static_cast<MockSurface*>(wl_resource_get_user_data(surface_resource));
  if (surface->xdg_surface) {
    uint32_t xdg_error = implementation == &xdg_surface_impl
                             ? XDG_SHELL_ERROR_ROLE
                             : ZXDG_SHELL_V6_ERROR_ROLE;
    wl_resource_post_error(resource, xdg_error, "surface already has a role");
    return;
  }
  wl_resource* xdg_surface_resource = wl_resource_create(
      client, interface, wl_resource_get_version(resource), id);

  if (!xdg_surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->xdg_surface.reset(
      new MockXdgSurface(xdg_surface_resource, implementation));
}

// xdg_shell

void GetXdgSurfaceV5(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &xdg_surface_interface, &xdg_surface_impl);
}

// zxdg_shell_v6

void GetXdgSurfaceV6(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &zxdg_surface_v6_interface, &zxdg_surface_v6_impl);
}

}  // namespace

ServerObject::ServerObject(wl_resource* resource) : resource_(resource) {}

ServerObject::~ServerObject() {
  if (resource_)
    wl_resource_destroy(resource_);
}

// static
void ServerObject::OnResourceDestroyed(wl_resource* resource) {
  auto* obj = static_cast<ServerObject*>(wl_resource_get_user_data(resource));
  obj->resource_ = nullptr;
}

MockXdgSurface::MockXdgSurface(wl_resource* resource,
                               const void* implementation)
    : ServerObject(resource) {
  wl_resource_set_implementation(resource, implementation, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockXdgSurface::~MockXdgSurface() {}

MockXdgTopLevel::MockXdgTopLevel(wl_resource* resource)
    : MockXdgSurface(resource, &zxdg_surface_v6_impl) {
  wl_resource_set_implementation(resource, &zxdg_toplevel_v6_impl, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockXdgTopLevel::~MockXdgTopLevel() {}

MockSurface::MockSurface(wl_resource* resource) : ServerObject(resource) {
  wl_resource_set_implementation(resource, &surface_impl, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockSurface::~MockSurface() {
  if (xdg_surface && xdg_surface->resource())
    wl_resource_destroy(xdg_surface->resource());
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!wl_resource_instance_of(resource, &wl_surface_interface, &surface_impl))
    return nullptr;
  return static_cast<MockSurface*>(wl_resource_get_user_data(resource));
}

MockPointer::MockPointer(wl_resource* resource) : ServerObject(resource) {
  wl_resource_set_implementation(resource, &pointer_impl, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockPointer::~MockPointer() {}

MockKeyboard::MockKeyboard(wl_resource* resource) : ServerObject(resource) {
  wl_resource_set_implementation(resource, &keyboard_impl, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockKeyboard::~MockKeyboard() {}

void GlobalDeleter::operator()(wl_global* global) {
  wl_global_destroy(global);
}

Global::Global(const wl_interface* interface,
               const void* implementation,
               uint32_t version)
    : interface_(interface),
      implementation_(implementation),
      version_(version) {}

Global::~Global() {}

bool Global::Initialize(wl_display* display) {
  global_.reset(wl_global_create(display, interface_, version_, this, &Bind));
  return global_ != nullptr;
}

// static
void Global::Bind(wl_client* client,
                  void* data,
                  uint32_t version,
                  uint32_t id) {
  auto* global = static_cast<Global*>(data);
  wl_resource* resource = wl_resource_create(
      client, global->interface_, std::min(version, global->version_), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  if (!global->resource_)
    global->resource_ = resource;
  wl_resource_set_implementation(resource, global->implementation_, global,
                                 &Global::OnResourceDestroyed);
  global->OnBind();
}

// static
void Global::OnResourceDestroyed(wl_resource* resource) {
  auto* global = static_cast<Global*>(wl_resource_get_user_data(resource));
  if (global->resource_ == resource)
    global->resource_ = nullptr;
}

MockCompositor::MockCompositor()
    : Global(&wl_compositor_interface, &compositor_impl, kCompositorVersion) {}

MockCompositor::~MockCompositor() {}

void MockCompositor::AddSurface(std::unique_ptr<MockSurface> surface) {
  surfaces_.push_back(std::move(surface));
}

MockOutput::MockOutput()
    : Global(&wl_output_interface, nullptr, kOutputVersion) {}

MockOutput::~MockOutput() {}

// Notify clients of the change for output position.
void MockOutput::OnBind() {
  const char* kUnknownMake = "unknown";
  const char* kUnknownModel = "unknown";
  wl_output_send_geometry(resource(), rect_.x(), rect_.y(), 0, 0, 0,
                          kUnknownMake, kUnknownModel, 0);
  wl_output_send_mode(resource(), WL_OUTPUT_MODE_CURRENT, rect_.width(),
                      rect_.height(), 0);
}

MockSeat::MockSeat() : Global(&wl_seat_interface, &seat_impl, kSeatVersion) {}

MockSeat::~MockSeat() {}

MockXdgShell::MockXdgShell()
    : Global(&xdg_shell_interface, &xdg_shell_impl, kXdgShellVersion) {}

MockXdgShell::~MockXdgShell() {}

MockXdgShellV6::MockXdgShellV6()
    : Global(&zxdg_shell_v6_interface, &zxdg_shell_v6_impl, kXdgShellVersion) {}

MockXdgShellV6::~MockXdgShellV6() {}

void DisplayDeleter::operator()(wl_display* display) {
  wl_display_destroy(display);
}

FakeServer::FakeServer()
    : Thread("fake_wayland_server"),
      pause_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      resume_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
      controller_(FROM_HERE) {}

FakeServer::~FakeServer() {
  Resume();
  Stop();
}

bool FakeServer::Start(uint32_t shell_version) {
  display_.reset(wl_display_create());
  if (!display_)
    return false;
  event_loop_ = wl_display_get_event_loop(display_.get());

  int fd[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd) < 0)
    return false;
  base::ScopedFD server_fd(fd[0]);
  base::ScopedFD client_fd(fd[1]);

  if (wl_display_init_shm(display_.get()) < 0)
    return false;
  if (!compositor_.Initialize(display_.get()))
    return false;
  if (!output_.Initialize(display_.get()))
    return false;
  if (!seat_.Initialize(display_.get()))
    return false;
  if (shell_version == 5) {
    if (!xdg_shell_.Initialize(display_.get()))
      return false;
  } else {
    if (!zxdg_shell_v6_.Initialize(display_.get()))
      return false;
  }

  client_ = wl_client_create(display_.get(), server_fd.get());
  if (!client_)
    return false;
  (void)server_fd.release();

  base::Thread::Options options;
  options.message_pump_factory =
      base::Bind(&FakeServer::CreateMessagePump, base::Unretained(this));
  if (!base::Thread::StartWithOptions(options))
    return false;

  setenv("WAYLAND_SOCKET", base::UintToString(client_fd.release()).c_str(), 1);

  return true;
}

void FakeServer::Pause() {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&FakeServer::DoPause, base::Unretained(this)));
  pause_event_.Wait();
}

void FakeServer::Resume() {
  if (display_)
    wl_display_flush_clients(display_.get());
  resume_event_.Signal();
}

void FakeServer::DoPause() {
  base::RunLoop().RunUntilIdle();
  pause_event_.Signal();
  resume_event_.Wait();
}

std::unique_ptr<base::MessagePump> FakeServer::CreateMessagePump() {
  auto pump = base::WrapUnique(new base::MessagePumpLibevent);
  pump->WatchFileDescriptor(wl_event_loop_get_fd(event_loop_), true,
                            base::MessagePumpLibevent::WATCH_READ, &controller_,
                            this);
  return std::move(pump);
}

void FakeServer::OnFileCanReadWithoutBlocking(int fd) {
  wl_event_loop_dispatch(event_loop_, 0);
  wl_display_flush_clients(display_.get());
}

void FakeServer::OnFileCanWriteWithoutBlocking(int fd) {}

}  // namespace wl
