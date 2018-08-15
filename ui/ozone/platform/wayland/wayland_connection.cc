// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection.h"

#include <drm_fourcc.h>

#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {
namespace {
const uint32_t kMaxCompositorVersion = 4;
const uint32_t kMaxLinuxDmabufVersion = 1;
const uint32_t kMaxSeatVersion = 4;
const uint32_t kMaxShmVersion = 1;
const uint32_t kMaxXdgShellVersion = 1;
}  // namespace

WaylandConnection::WaylandConnection()
    : controller_(FROM_HERE), binding_(this) {}

WaylandConnection::~WaylandConnection() {
  DCHECK(pending_buffer_map_.empty() && params_to_id_map_.empty() &&
         buffers_.empty());
}

bool WaylandConnection::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandConnection::Global, &WaylandConnection::GlobalRemove,
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

  while (!PrimaryOutput() || !PrimaryOutput()->is_ready())
    wl_display_roundtrip(display_.get());

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!seat_) {
    LOG(ERROR) << "No wl_seat object";
    return false;
  }
  if (!shell_v6_ && !shell_) {
    LOG(ERROR) << "No xdg_shell object";
    return false;
  }

  return true;
}

bool WaylandConnection::StartProcessingEvents() {
  if (watching_)
    return true;

  DCHECK(display_);
  wl_display_flush(display_.get());

  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
          wl_display_get_fd(display_.get()), true,
          base::MessagePumpLibevent::WATCH_READ, &controller_, this))
    return false;

  watching_ = true;
  return true;
}

void WaylandConnection::ScheduleFlush() {
  if (scheduled_flush_ || !watching_)
    return;
  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnection::Flush, base::Unretained(this)));
  scheduled_flush_ = true;
}

WaylandWindow* WaylandConnection::GetWindow(gfx::AcceleratedWidget widget) {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

WaylandWindow* WaylandConnection::GetCurrentFocusedWindow() {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus())
      return window;
  }
  return nullptr;
}

void WaylandConnection::AddWindow(gfx::AcceleratedWidget widget,
                                  WaylandWindow* window) {
  window_map_[widget] = window;
}

void WaylandConnection::RemoveWindow(gfx::AcceleratedWidget widget) {
  if (touch_)
    touch_->RemoveTouchPoints(window_map_[widget]);
  window_map_.erase(widget);
}

WaylandOutput* WaylandConnection::PrimaryOutput() const {
  if (!output_list_.size())
    return nullptr;
  return output_list_.front().get();
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!pointer_ || !pointer_->cursor())
    return;
  pointer_->cursor()->UpdateBitmap(bitmaps, location, serial_);
}

int WaylandConnection::GetKeyboardModifiers() {
  int modifiers = 0;
  if (keyboard_)
    modifiers = keyboard_->modifiers();
  return modifiers;
}

// TODO(msisov): handle buffer swap failure or success.
void WaylandConnection::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!ValidateDataFromGpu(widget, buffer_id))
    return;

  auto it = buffers_.find(buffer_id);
  // A buffer might not exist by this time. So, store the request and process
  // it once it is created.
  if (it == buffers_.end()) {
    auto pending_buffers_it = pending_buffer_map_.find(buffer_id);
    if (pending_buffers_it != pending_buffer_map_.end()) {
      // If a buffer didn't exist and second call for a swap comes, buffer must
      // be associated with the same widget.
      DCHECK_EQ(pending_buffers_it->second, widget);
    } else {
      pending_buffer_map_.insert(
          std::pair<uint32_t, gfx::AcceleratedWidget>(buffer_id, widget));
    }
    return;
  }
  struct wl_buffer* buffer = it->second.get();

  WaylandWindow* window = GetWindow(widget);
  if (!window)
    return;

  // TODO(msisov): it would be beneficial to use real damage regions to improve
  // performance.
  //
  // TODO(msisov): also start using wl_surface_frame callbacks for better
  // performance.
  wl_surface_damage(window->surface(), 0, 0, window->GetBounds().width(),
                    window->GetBounds().height());
  wl_surface_attach(window->surface(), buffer, 0, 0);
  wl_surface_commit(window->surface());

  ScheduleFlush();
}

void WaylandConnection::CreateZwpLinuxDmabuf(
    base::File file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  TRACE_EVENT2("Wayland", "WaylandConnection::CreateZwpLinuxDmabuf", "Format",
               format, "Buffer id", buffer_id);

  static const struct zwp_linux_buffer_params_v1_listener params_listener = {
      WaylandConnection::CreateSucceeded, WaylandConnection::CreateFailed};

  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!ValidateDataFromGpu(file, width, height, strides, offsets, format,
                           modifiers, planes_count, buffer_id)) {
    // base::File::Close() has an assertion that checks if blocking operations
    // are allowed. Thus, manually close the fd here.
    base::ScopedFD fd(file.TakePlatformFile());
    fd.reset();
    return;
  }

  // Store |params| connected to |buffer_id| to track buffer creation and
  // identify, which buffer a client wants to use.
  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf());
  params_to_id_map_.insert(
      std::pair<struct zwp_linux_buffer_params_v1*, uint32_t>(params,
                                                              buffer_id));
  uint32_t fd = file.TakePlatformFile();
  for (size_t i = 0; i < planes_count; i++) {
    zwp_linux_buffer_params_v1_add(params, fd, i /* plane id */, offsets[i],
                                   strides[i], 0 /* modifier hi */,
                                   0 /* modifier lo */);
  }
  zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);
  zwp_linux_buffer_params_v1_create(params, width, height, format, 0);

  ScheduleFlush();
}

void WaylandConnection::DestroyZwpLinuxDmabuf(uint32_t buffer_id) {
  TRACE_EVENT1("Wayland", "WaylandConnection::DestroyZwpLinuxDmabuf",
               "Buffer id", buffer_id);

  DCHECK(base::MessageLoopForUI::IsCurrent());

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    TerminateGpuProcess("Trying to destroy non-existing buffer");
    return;
  }
  buffers_.erase(it);

  ScheduleFlush();
}

ClipboardDelegate* WaylandConnection::GetClipboardDelegate() {
  return this;
}

// static
void WaylandConnection::CreateSucceeded(
    void* data,
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  DCHECK(connection);

  // Find which buffer id |params| belong to and store wl_buffer
  // with that id.
  auto it = connection->params_to_id_map_.find(params);
  CHECK(it != connection->params_to_id_map_.end());
  uint32_t buffer_id = it->second;
  connection->params_to_id_map_.erase(params);
  zwp_linux_buffer_params_v1_destroy(params);

  connection->buffers_.insert(std::pair<uint32_t, wl::Object<wl_buffer>>(
      buffer_id, wl::Object<wl_buffer>(new_buffer)));

  TRACE_EVENT1("Wayland", "WaylandConnection::CreateSucceeded", "Buffer id",
               buffer_id);

  auto pending_buffers_it = connection->pending_buffer_map_.find(buffer_id);
  if (pending_buffers_it != connection->pending_buffer_map_.end()) {
    gfx::AcceleratedWidget widget = pending_buffers_it->second;
    connection->pending_buffer_map_.erase(pending_buffers_it);
    connection->ScheduleBufferSwap(widget, buffer_id);
  }
}

// static
void WaylandConnection::CreateFailed(
    void* data,
    struct zwp_linux_buffer_params_v1* params) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  zwp_linux_buffer_params_v1_destroy(params);
  LOG(FATAL) << "zwp_linux_buffer_params.create failed";
}

void WaylandConnection::OfferClipboardData(
    const ClipboardDelegate::DataMap& data_map,
    ClipboardDelegate::OfferDataClosure callback) {
  if (!data_source_) {
    wl_data_source* data_source = data_device_manager_->CreateSource();
    data_source_.reset(new WaylandDataSource(data_source));
    data_source_->set_connection(this);
    data_source_->WriteToClipboard(data_map);
  }
  data_source_->UpdataDataMap(data_map);
  std::move(callback).Run();
}

void WaylandConnection::RequestClipboardData(
    const std::string& mime_type,
    ClipboardDelegate::DataMap* data_map,
    ClipboardDelegate::RequestDataClosure callback) {
  read_clipboard_closure_ = std::move(callback);

  DCHECK(data_map);
  data_map_ = data_map;
  data_device_->RequestSelectionData(mime_type);
}

bool WaylandConnection::IsSelectionOwner() {
  return !!data_source_;
}

ozone::mojom::WaylandConnectionPtr WaylandConnection::BindInterface() {
  // This mustn't be called twice or when the zwp_linux_dmabuf interface is not
  // available.
  DCHECK(!binding_.is_bound() || zwp_linux_dmabuf_);
  ozone::mojom::WaylandConnectionPtr ptr;
  binding_.Bind(MakeRequest(&ptr));
  return ptr;
}

std::vector<gfx::BufferFormat> WaylandConnection::GetSupportedBufferFormats() {
  return buffer_formats_;
}

void WaylandConnection::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

void WaylandConnection::GetAvailableMimeTypes(
    ClipboardDelegate::GetMimeTypesClosure callback) {
  std::move(callback).Run(data_device_->GetAvailableMimeTypes());
}

void WaylandConnection::DataSourceCancelled() {
  SetClipboardData(std::string(), std::string());
  data_source_.reset();
}

void WaylandConnection::SetClipboardData(const std::string& contents,
                                         const std::string& mime_type) {
  if (!data_map_)
    return;

  (*data_map_)[mime_type] =
      std::vector<uint8_t>(contents.begin(), contents.end());

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
}

void WaylandConnection::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandConnection::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandConnection::DispatchUiEvent(Event* event) {
  PlatformEventSource::DispatchEvent(event);
}

void WaylandConnection::OnFileCanReadWithoutBlocking(int fd) {
  wl_display_dispatch(display_.get());
  for (const auto& window : window_map_)
    window.second->ApplyPendingBounds();
}

void WaylandConnection::OnFileCanWriteWithoutBlocking(int fd) {}

bool WaylandConnection::ValidateDataFromGpu(
    const base::File& file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  std::string reason;
  if (!file.IsValid())
    reason = "Buffer fd is invalid";

  if (width == 0 || height == 0)
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = "Number of strides(" + std::to_string(strides.size()) +
             ")/offsets(" + std::to_string(offsets.size()) + ")/modifiers(" +
             std::to_string(modifiers.size()) +
             ") does not correspond to the number of planes(" +
             std::to_string(planes_count) + ")";
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  if (!reason.empty()) {
    TerminateGpuProcess(reason);
    return false;
  }
  return true;
}

bool WaylandConnection::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    uint32_t buffer_id) {
  std::string reason;

  if (widget == gfx::kNullAcceleratedWidget)
    reason = "Invalid accelerated widget";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  if (!reason.empty()) {
    TerminateGpuProcess(reason);
    return false;
  }

  return true;
}

void WaylandConnection::TerminateGpuProcess(std::string reason) {
  std::move(terminate_gpu_cb_).Run(std::move(reason));
  binding_.Unbind();

  buffers_.clear();
  params_to_id_map_.clear();
  pending_buffer_map_.clear();
}

const std::vector<std::unique_ptr<WaylandOutput>>&
WaylandConnection::GetOutputList() const {
  return output_list_;
}

// static
void WaylandConnection::Global(void* data,
                               wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandConnection::Capabilities, &WaylandConnection::Name,
  };
  static const xdg_shell_listener shell_listener = {
      &WaylandConnection::Ping,
  };
  static const zxdg_shell_v6_listener shell_v6_listener = {
      &WaylandConnection::PingV6,
  };
  static const zwp_linux_dmabuf_v1_listener dmabuf_listener = {
      &WaylandConnection::Format, &WaylandConnection::Modifiers,
  };

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (!connection->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    connection->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    if (!connection->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!connection->subcompositor_ &&
             strcmp(interface, "wl_subcompositor") == 0) {
    connection->subcompositor_ = wl::Bind<wl_subcompositor>(registry, name, 1);
    if (!connection->subcompositor_)
      LOG(ERROR) << "Failed to bind to wl_subcompositor global";
  } else if (!connection->shm_ && strcmp(interface, "wl_shm") == 0) {
    connection->shm_ =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
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

    // TODO(tonikitoo,msisov): The connection passed to WaylandInputDevice must
    // have a valid data device manager. We should ideally be robust to the
    // compositor advertising a wl_seat first. No known compositor does this,
    // fortunately.
    if (!connection->data_device_manager_) {
      LOG(ERROR)
          << "No data device manager. Clipboard won't be fully functional";
      return;
    }
    wl_data_device* data_device = connection->data_device_manager_->GetDevice();
    connection->data_device_.reset(
        new WaylandDataDevice(connection, data_device));
  } else if (!connection->shell_v6_ &&
             strcmp(interface, "zxdg_shell_v6") == 0) {
    // Check for zxdg_shell_v6 first.
    connection->shell_v6_ = wl::Bind<zxdg_shell_v6>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_v6_) {
      LOG(ERROR) << "Failed to  bind to zxdg_shell_v6 global";
      return;
    }
    zxdg_shell_v6_add_listener(connection->shell_v6_.get(), &shell_v6_listener,
                               connection);
  } else if (!connection->shell_v6_ && !connection->shell_ &&
             strcmp(interface, "xdg_shell") == 0) {
    connection->shell_ = wl::Bind<xdg_shell>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_) {
      LOG(ERROR) << "Failed to  bind to xdg_shell global";
      return;
    }
    xdg_shell_add_listener(connection->shell_.get(), &shell_listener,
                           connection);
    xdg_shell_use_unstable_version(connection->shell_.get(),
                                   XDG_SHELL_VERSION_CURRENT);
  } else if (base::EqualsCaseInsensitiveASCII(interface, "wl_output")) {
    wl::Object<wl_output> output = wl::Bind<wl_output>(registry, name, 1);
    if (!output) {
      LOG(ERROR) << "Failed to bind to wl_output global";
      return;
    }

    if (!connection->output_list_.empty())
      NOTIMPLEMENTED() << "Multiple screens support is not implemented";

    connection->output_list_.push_back(base::WrapUnique(new WaylandOutput(
        connection->get_next_display_id(), output.release())));
  } else if (!connection->data_device_manager_ &&
             strcmp(interface, "wl_data_device_manager") == 0) {
    wl::Object<wl_data_device_manager> data_device_manager =
        wl::Bind<wl_data_device_manager>(registry, name, 1);
    if (!data_device_manager) {
      LOG(ERROR) << "Failed to bind to wl_data_device_manager global";
      return;
    }
    connection->data_device_manager_.reset(
        new WaylandDataDeviceManager(data_device_manager.release()));
    connection->data_device_manager_->set_connection(connection);
  } else if (!connection->zwp_linux_dmabuf_ &&
             (strcmp(interface, "zwp_linux_dmabuf_v1") == 0)) {
    connection->zwp_linux_dmabuf_ = wl::Bind<zwp_linux_dmabuf_v1>(
        registry, name, std::min(version, kMaxLinuxDmabufVersion));
    zwp_linux_dmabuf_v1_add_listener(connection->zwp_linux_dmabuf(),
                                     &dmabuf_listener, connection);
    // A roundtrip after binding guarantees that the client has received all
    // supported formats.
    wl_display_roundtrip(connection->display_.get());
  }

  connection->ScheduleFlush();
}

// static
void WaylandConnection::GlobalRemove(void* data,
                                     wl_registry* registry,
                                     uint32_t name) {
  NOTIMPLEMENTED();
}

// static
void WaylandConnection::Capabilities(void* data,
                                     wl_seat* seat,
                                     uint32_t capabilities) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!connection->pointer_) {
      wl_pointer* pointer = wl_seat_get_pointer(connection->seat_.get());
      if (!pointer) {
        LOG(ERROR) << "Failed to get wl_pointer from seat";
        return;
      }
      connection->pointer_ = std::make_unique<WaylandPointer>(
          pointer, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                                       base::Unretained(connection)));
      connection->pointer_->set_connection(connection);
    }
  } else if (connection->pointer_) {
    connection->pointer_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!connection->keyboard_) {
      wl_keyboard* keyboard = wl_seat_get_keyboard(connection->seat_.get());
      if (!keyboard) {
        LOG(ERROR) << "Failed to get wl_keyboard from seat";
        return;
      }
      connection->keyboard_ = std::make_unique<WaylandKeyboard>(
          keyboard, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                                        base::Unretained(connection)));
      connection->keyboard_->set_connection(connection);
    }
  } else if (connection->keyboard_) {
    connection->keyboard_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
    if (!connection->touch_) {
      wl_touch* touch = wl_seat_get_touch(connection->seat_.get());
      if (!touch) {
        LOG(ERROR) << "Failed to get wl_touch from seat";
        return;
      }
      connection->touch_ = std::make_unique<WaylandTouch>(
          touch, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                                     base::Unretained(connection)));
      connection->touch_->set_connection(connection);
    }
  } else if (connection->touch_) {
    connection->touch_.reset();
  }
  connection->ScheduleFlush();
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
void WaylandConnection::Ping(void* data, xdg_shell* shell, uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  xdg_shell_pong(shell, serial);
  connection->ScheduleFlush();
}

// static
void WaylandConnection::Modifiers(void* data,
                                  struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                  uint32_t format,
                                  uint32_t modifier_hi,
                                  uint32_t modifier_lo) {
  NOTIMPLEMENTED();
}

// static
void WaylandConnection::Format(void* data,
                               struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                               uint32_t format) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  // Return on not the supported ARGB format.
  if (format == DRM_FORMAT_ARGB2101010)
    return;
  connection->buffer_formats_.push_back(
      GetBufferFormatFromFourCCFormat(format));
}

}  // namespace ui
