// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

namespace ui {

namespace {
constexpr uint32_t kMaxCompositorVersion = 4;
constexpr uint32_t kMaxGtkPrimarySelectionDeviceManagerVersion = 1;
constexpr uint32_t kMaxLinuxDmabufVersion = 3;
constexpr uint32_t kMaxSeatVersion = 4;
constexpr uint32_t kMaxShmVersion = 1;
constexpr uint32_t kMaxXdgShellVersion = 1;
constexpr uint32_t kMaxDeviceManagerVersion = 3;
constexpr uint32_t kMaxWpPresentationVersion = 1;
constexpr uint32_t kMaxTextInputManagerVersion = 1;
constexpr uint32_t kMinWlDrmVersion = 2;
constexpr uint32_t kMinWlOutputVersion = 2;
}  // namespace

WaylandConnection::WaylandConnection() = default;

WaylandConnection::~WaylandConnection() = default;

bool WaylandConnection::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandConnection::Global,
      &WaylandConnection::GlobalRemove,
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

  // Now that the connection with the display server has been properly
  // estabilished, initialize the event source and input objects.
  DCHECK(!event_source_);
  event_source_ =
      std::make_unique<WaylandEventSource>(display(), wayland_window_manager());

  wl_registry_add_listener(registry_.get(), &registry_listener, this);
  while (!wayland_output_manager_ ||
         !wayland_output_manager_->IsOutputReady()) {
    wl_display_roundtrip(display_.get());
  }

  buffer_manager_host_ = std::make_unique<WaylandBufferManagerHost>(this);

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!shell_v6_ && !shell_) {
    LOG(ERROR) << "No Wayland shell found";
    return false;
  }

  // When we are running tests with weston in headless mode, the seat is not
  // announced.
  if (!seat_)
    LOG(WARNING) << "No wl_seat object. The functionality may suffer.";

  return true;
}

void WaylandConnection::ScheduleFlush() {
  // When we are in tests, the message loop is set later when the
  // initialization of the OzonePlatform complete. Thus, just
  // flush directly. This doesn't happen in normal run.
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    Flush();
  } else if (!scheduled_flush_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandConnection::Flush, base::Unretained(this)));
    scheduled_flush_ = true;
  }
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!cursor_)
    return;
  cursor_->UpdateBitmap(bitmaps, location, serial_);
}

void WaylandConnection::StartDrag(const ui::OSExchangeData& data,
                                  int operation) {
  if (!dragdrop_data_source_)
    dragdrop_data_source_ = data_device_manager_->CreateSource();
  dragdrop_data_source_->Offer(data);
  dragdrop_data_source_->SetAction(operation);
  data_device_->StartDrag(dragdrop_data_source_->data_source(), data);
}

void WaylandConnection::FinishDragSession(uint32_t dnd_action,
                                          WaylandWindow* source_window) {
  if (source_window)
    source_window->OnDragSessionClose(dnd_action);
  data_device_->ResetSourceData();
  dragdrop_data_source_.reset();
}

void WaylandConnection::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  data_device_->DeliverDragData(mime_type, buffer);
}

void WaylandConnection::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  data_device_->RequestDragData(mime_type, std::move(callback));
}

bool WaylandConnection::IsDragInProgress() {
  // |data_device_| can be null when running on headless weston.
  if (!data_device_)
    return false;
  return data_device_->IsDragEntered() || drag_data_source();
}

void WaylandConnection::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandConnection::UpdateInputDevices(wl_seat* seat,
                                           uint32_t capabilities) {
  DCHECK(seat);
  DCHECK(event_source_);
  auto has_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
  auto has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  auto has_touch = capabilities & WL_SEAT_CAPABILITY_TOUCH;

  if (!has_pointer) {
    pointer_.reset();
    cursor_.reset();
    wayland_cursor_position_.reset();
  } else if (wl_pointer* pointer = wl_seat_get_pointer(seat)) {
    pointer_ = std::make_unique<WaylandPointer>(pointer, this, event_source());
    cursor_ = std::make_unique<WaylandCursor>(pointer_.get(), this);
    wayland_cursor_position_ = std::make_unique<WaylandCursorPosition>();
  } else {
    LOG(ERROR) << "Failed to get wl_pointer from seat";
  }

  if (!has_keyboard) {
    keyboard_.reset();
  } else if (wl_keyboard* keyboard = wl_seat_get_keyboard(seat)) {
    auto* layout_engine =
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
    keyboard_ = std::make_unique<WaylandKeyboard>(keyboard, this, layout_engine,
                                                  event_source());
  } else {
    LOG(ERROR) << "Failed to get wl_keyboard from seat";
  }

  if (!has_touch) {
    touch_.reset();
  } else if (wl_touch* touch = wl_seat_get_touch(seat)) {
    touch_ = std::make_unique<WaylandTouch>(touch, this, event_source());
  } else {
    LOG(ERROR) << "Failed to get wl_touch from seat";
  }
}

void WaylandConnection::EnsureDataDevice() {
  if (!data_device_manager_ || !seat_)
    return;
  DCHECK(!data_device_);
  wl_data_device* data_device = data_device_manager_->GetDevice();
  data_device_ = std::make_unique<WaylandDataDevice>(this, data_device);

  if (primary_selection_device_manager_) {
    primary_selection_device_ = std::make_unique<GtkPrimarySelectionDevice>(
        this, primary_selection_device_manager_->GetDevice());
  }

  clipboard_ = std::make_unique<WaylandClipboard>(
      data_device_manager_.get(), data_device_.get(),
      primary_selection_device_manager_.get(), primary_selection_device_.get());
}

// static
void WaylandConnection::Global(void* data,
                               wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandConnection::Capabilities,
      &WaylandConnection::Name,
  };
  static const xdg_wm_base_listener shell_listener = {
      &WaylandConnection::Ping,
  };
  static const zxdg_shell_v6_listener shell_v6_listener = {
      &WaylandConnection::PingV6,
  };

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (!connection->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    connection->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    connection->compositor_version_ = version;
    if (!connection->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!connection->subcompositor_ &&
             strcmp(interface, "wl_subcompositor") == 0) {
    connection->subcompositor_ = wl::Bind<wl_subcompositor>(registry, name, 1);
    if (!connection->subcompositor_)
      LOG(ERROR) << "Failed to bind to wl_subcompositor global";
  } else if (!connection->shm_ && strcmp(interface, "wl_shm") == 0) {
    wl::Object<wl_shm> shm =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
    connection->shm_ = std::make_unique<WaylandShm>(shm.release(), connection);
    if (!connection->shm_)
      LOG(ERROR) << "Failed to bind to wl_shm global";
  } else if (!connection->seat_ && strcmp(interface, "wl_seat") == 0) {
    connection->seat_ =
        wl::Bind<wl_seat>(registry, name, std::min(version, kMaxSeatVersion));
    if (!connection->seat_) {
      LOG(ERROR) << "Failed to bind to wl_seat global";
      return;
    }
    wl_seat_add_listener(connection->seat_.get(), &seat_listener, connection);
    connection->EnsureDataDevice();
  } else if (!connection->shell_v6_ &&
             strcmp(interface, "zxdg_shell_v6") == 0) {
    // Check for zxdg_shell_v6 first.
    connection->shell_v6_ = wl::Bind<zxdg_shell_v6>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_v6_) {
      LOG(ERROR) << "Failed to bind to zxdg_shell_v6 global";
      return;
    }
    zxdg_shell_v6_add_listener(connection->shell_v6_.get(), &shell_v6_listener,
                               connection);
  } else if (!connection->shell_v6_ && !connection->shell_ &&
             strcmp(interface, "xdg_wm_base") == 0) {
    connection->shell_ = wl::Bind<xdg_wm_base>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_) {
      LOG(ERROR) << "Failed to bind to xdg_wm_base global";
      return;
    }
    xdg_wm_base_add_listener(connection->shell_.get(), &shell_listener,
                             connection);
  } else if (base::EqualsCaseInsensitiveASCII(interface, "wl_output")) {
    if (version < kMinWlOutputVersion) {
      LOG(ERROR)
          << "Unable to bind to the unsupported wl_output object with version= "
          << version << ". Minimum supported version is "
          << kMinWlOutputVersion;
      return;
    }

    wl::Object<wl_output> output = wl::Bind<wl_output>(registry, name, version);
    if (!output) {
      LOG(ERROR) << "Failed to bind to wl_output global";
      return;
    }

    if (!connection->wayland_output_manager_) {
      connection->wayland_output_manager_ =
          std::make_unique<WaylandOutputManager>();
    }
    connection->wayland_output_manager_->AddWaylandOutput(name,
                                                          output.release());
  } else if (!connection->data_device_manager_ &&
             strcmp(interface, "wl_data_device_manager") == 0) {
    wl::Object<wl_data_device_manager> data_device_manager =
        wl::Bind<wl_data_device_manager>(
            registry, name, std::min(version, kMaxDeviceManagerVersion));
    if (!data_device_manager) {
      LOG(ERROR) << "Failed to bind to wl_data_device_manager global";
      return;
    }
    connection->data_device_manager_ =
        std::make_unique<WaylandDataDeviceManager>(
            data_device_manager.release(), connection);
    connection->EnsureDataDevice();
  } else if (!connection->primary_selection_device_manager_ &&
             strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    wl::Object<gtk_primary_selection_device_manager> manager =
        wl::Bind<gtk_primary_selection_device_manager>(
            registry, name, kMaxGtkPrimarySelectionDeviceManagerVersion);
    connection->primary_selection_device_manager_ =
        std::make_unique<GtkPrimarySelectionDeviceManager>(manager.release(),
                                                           connection);
  } else if (!connection->zwp_dmabuf_ &&
             (strcmp(interface, "zwp_linux_dmabuf_v1") == 0)) {
    wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf =
        wl::Bind<zwp_linux_dmabuf_v1>(
            registry, name, std::min(version, kMaxLinuxDmabufVersion));
    connection->zwp_dmabuf_ = std::make_unique<WaylandZwpLinuxDmabuf>(
        zwp_linux_dmabuf.release(), connection);
  } else if (!connection->presentation_ &&
             (strcmp(interface, "wp_presentation") == 0)) {
    connection->presentation_ =
        wl::Bind<wp_presentation>(registry, name, kMaxWpPresentationVersion);
  } else if (!connection->text_input_manager_v1_ &&
             strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    connection->text_input_manager_v1_ = wl::Bind<zwp_text_input_manager_v1>(
        registry, name, std::min(version, kMaxTextInputManagerVersion));
    if (!connection->text_input_manager_v1_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v1 global";
      return;
    }
  } else if (!connection->drm_ && (strcmp(interface, "wl_drm") == 0) &&
             version >= kMinWlDrmVersion) {
    auto wayland_drm = wl::Bind<struct wl_drm>(registry, name, version);
    connection->drm_ =
        std::make_unique<WaylandDrm>(wayland_drm.release(), connection);
  }

  connection->ScheduleFlush();
}

// static
void WaylandConnection::GlobalRemove(void* data,
                                     wl_registry* registry,
                                     uint32_t name) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  // The Wayland protocol distinguishes global objects by unique numeric names,
  // which the WaylandOutputManager uses as unique output ids. But, it is only
  // possible to figure out, what global object is going to be removed on the
  // WaylandConnection::GlobalRemove call. Thus, whatever unique |name| comes,
  // it's forwarded to the WaylandOutputManager, which checks if such a global
  // output object exists and removes it.
  if (connection->wayland_output_manager_)
    connection->wayland_output_manager_->RemoveWaylandOutput(name);
}

// static
void WaylandConnection::Capabilities(void* data,
                                     wl_seat* seat,
                                     uint32_t capabilities) {
  WaylandConnection* self = static_cast<WaylandConnection*>(data);
  DCHECK(self);
  self->UpdateInputDevices(seat, capabilities);
  self->ScheduleFlush();
}

// static
void WaylandConnection::Name(void* data, wl_seat* seat, const char* name) {}

// static
void WaylandConnection::PingV6(void* data,
                               zxdg_shell_v6* shell_v6,
                               uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  zxdg_shell_v6_pong(shell_v6, serial);
  connection->ScheduleFlush();
}

// static
void WaylandConnection::Ping(void* data, xdg_wm_base* shell, uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  xdg_wm_base_pong(shell, serial);
  connection->ScheduleFlush();
}

}  // namespace ui
