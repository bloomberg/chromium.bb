// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_data_device.h"

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_util.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

namespace {

constexpr char kMimeTypeText[] = "text/plain";
constexpr char kMimeTypeTextUTF8[] = "text/plain;charset=utf-8";

int GetOperation(uint32_t source_actions, uint32_t dnd_action) {
  uint32_t action = dnd_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE
                        ? dnd_action
                        : source_actions;

  int operation = DragDropTypes::DRAG_NONE;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    operation |= DragDropTypes::DRAG_COPY;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    operation |= DragDropTypes::DRAG_MOVE;
  // TODO(jkim): Implement branch for WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    operation |= DragDropTypes::DRAG_COPY;
  return operation;
}

void AddStringToOSExchangeData(const std::string& data,
                               OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);
  if (data.empty())
    return;

  base::string16 string16 = base::UTF8ToUTF16(data);
  os_exchange_data->SetString(string16);
}

void AddToOSExchangeData(const std::string& data,
                         const std::string& mime_type,
                         OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);
  if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUTF8)) {
    DCHECK(!os_exchange_data->HasString());
    AddStringToOSExchangeData(data, os_exchange_data);
    return;
  }

  // TODO(jkim): Handle other mime types as well.
  NOTREACHED();
}

}  // namespace

// static
const wl_callback_listener WaylandDataDevice::callback_listener_ = {
    WaylandDataDevice::SyncCallback,
};

WaylandDataDevice::WaylandDataDevice(WaylandConnection* connection,
                                     wl_data_device* data_device)
    : data_device_(data_device),
      connection_(connection),
      shared_memory_(new base::SharedMemory()) {
  static const struct wl_data_device_listener kDataDeviceListener = {
      WaylandDataDevice::OnDataOffer, WaylandDataDevice::OnEnter,
      WaylandDataDevice::OnLeave,     WaylandDataDevice::OnMotion,
      WaylandDataDevice::OnDrop,      WaylandDataDevice::OnSelection};
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);
}

WaylandDataDevice::~WaylandDataDevice() {
  if (!shared_memory_->handle().GetHandle())
    return;
  shared_memory_->Unmap();
  shared_memory_->Close();
}

void WaylandDataDevice::RequestSelectionData(const std::string& mime_type) {
  base::ScopedFD fd = selection_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  read_from_fd_closure_ =
      base::BindOnce(&WaylandDataDevice::ReadClipboardDataFromFD,
                     base::Unretained(this), std::move(fd), mime_type);
  RegisterSyncCallback();
}

void WaylandDataDevice::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  base::ScopedFD fd = drag_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  read_from_fd_closure_ = base::BindOnce(&WaylandDataDevice::ReadDragDataFromFD,
                                         base::Unretained(this), std::move(fd),
                                         std::move(callback));
  RegisterSyncCallback();
}

void WaylandDataDevice::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  DCHECK(buffer);
  DCHECK(source_data_);

  if (mime_type != kMimeTypeText && mime_type != kMimeTypeTextUTF8)
    return;

  const OSExchangeData::FilenameToURLPolicy policy =
      OSExchangeData::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES;
  // TODO(jkim): Handle other data format as well.
  if (source_data_->HasURL(policy)) {
    GURL url;
    base::string16 title;
    source_data_->GetURLAndTitle(policy, &url, &title);
    buffer->append(url.spec());
    return;
  }

  if (source_data_->HasString()) {
    base::string16 data;
    source_data_->GetString(&data);
    buffer->append(base::UTF16ToUTF8(data));
    return;
  }
}

void WaylandDataDevice::StartDrag(const wl_data_source& data_source,
                                  const ui::OSExchangeData& data) {
  WaylandWindow* window = connection_->GetCurrentFocusedWindow();
  if (!window) {
    LOG(ERROR) << "Failed to get focused window.";
    return;
  }

  wl_surface* surface = window->surface();
  const SkBitmap* icon = data.provider().GetDragImage().bitmap();
  if (icon && !icon->empty())
    CreateDragImage(icon);

  source_data_ = std::make_unique<ui::OSExchangeData>(data.provider().Clone());
  wl_data_device_start_drag(data_device_.get(),
                            const_cast<wl_data_source*>(&data_source), surface,
                            icon_surface_.get(), connection_->serial());
  connection_->ScheduleFlush();
}

void WaylandDataDevice::ResetSourceData() {
  source_data_.reset();
}

std::vector<std::string> WaylandDataDevice::GetAvailableMimeTypes() {
  if (selection_offer_)
    return selection_offer_->GetAvailableMimeTypes();

  return std::vector<std::string>();
}

void WaylandDataDevice::ReadClipboardDataFromFD(base::ScopedFD fd,
                                                const std::string& mime_type) {
  std::string contents;
  ReadDataFromFD(std::move(fd), &contents);
  connection_->SetClipboardData(contents, mime_type);
}

void WaylandDataDevice::ReadDragDataFromFD(
    base::ScopedFD fd,
    base::OnceCallback<void(const std::string&)> callback) {
  std::string contents;
  ReadDataFromFD(std::move(fd), &contents);
  std::move(callback).Run(contents);
}

void WaylandDataDevice::RegisterSyncCallback() {
  DCHECK(!sync_callback_);
  sync_callback_.reset(wl_display_sync(connection_->display()));
  wl_callback_add_listener(sync_callback_.get(), &callback_listener_, this);
  wl_display_flush(connection_->display());
}

void WaylandDataDevice::ReadDataFromFD(base::ScopedFD fd,
                                       std::string* contents) {
  DCHECK(contents);
  char buffer[1 << 10];  // 1 kB in bytes.
  ssize_t length;
  while ((length = read(fd.get(), buffer, sizeof(buffer))) > 0)
    contents->append(buffer, length);
}

void WaylandDataDevice::HandleDeferredLeaveIfNeeded() {
  if (!is_leaving_)
    return;

  OnLeave(this, data_device_.get());
}

// static
void WaylandDataDevice::OnDataOffer(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);

  DCHECK(!self->new_offer_);
  self->new_offer_.reset(new WaylandDataOffer(offer));
}

void WaylandDataDevice::OnEnter(void* data,
                                wl_data_device* data_device,
                                uint32_t serial,
                                wl_surface* surface,
                                wl_fixed_t x,
                                wl_fixed_t y,
                                wl_data_offer* offer) {
  WaylandWindow* window =
      static_cast<WaylandWindow*>(wl_surface_get_user_data(surface));
  if (!window) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self->new_offer_);
  DCHECK(!self->drag_offer_);
  self->drag_offer_ = std::move(self->new_offer_);
  self->window_ = window;

  // TODO(jkim): Set mime type the client can accept. Now it sets all mime types
  // offered because current implementation doesn't decide action based on mime
  // type.
  const std::vector<std::string>& mime_types =
      self->drag_offer_->GetAvailableMimeTypes();
  for (auto mime : mime_types)
    self->drag_offer_->Accept(serial, mime);

  std::copy(mime_types.begin(), mime_types.end(),
            std::insert_iterator<std::list<std::string>>(
                self->unprocessed_mime_types_,
                self->unprocessed_mime_types_.begin()));

  int operation = GetOperation(self->drag_offer_->source_actions(),
                               self->drag_offer_->dnd_action());
  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));

  // If it has |source_data_|, it means that the dragging is started from the
  // same window and it doesn't need to read the data through Wayland.
  if (self->source_data_) {
    std::unique_ptr<OSExchangeData> data = std::make_unique<OSExchangeData>(
        self->source_data_->provider().Clone());
    self->window_->OnDragEnter(point, std::move(data), operation);
    return;
  }

  self->window_->OnDragEnter(point, nullptr, operation);
}

void WaylandDataDevice::OnMotion(void* data,
                                 wl_data_device* data_device,
                                 uint32_t time,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  int operation = GetOperation(self->drag_offer_->source_actions(),
                               self->drag_offer_->dnd_action());
  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));
  int client_operation = self->window_->OnDragMotion(point, time, operation);
  self->SetOperation(client_operation);
}

void WaylandDataDevice::OnDrop(void* data, wl_data_device* data_device) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  // Creates buffer to receive data from Wayland.
  self->received_data_ = std::make_unique<OSExchangeData>(
      std::make_unique<OSExchangeDataProviderAura>());

  // Starts to read the data on Drop event because read(..) API blocks
  // awaiting data to be sent to pipe if we try to read the data on OnEnter.
  // 'Weston' also reads data on OnDrop event and other examples do as well.
  self->HandleNextMimeType();

  // In order to guarantee all data received, it sets
  // |is_handling_dropped_data_| and defers OnLeave event handling if it gets
  // OnLeave event before completing to read the data.
  self->is_handling_dropped_data_ = true;
}

void WaylandDataDevice::OnLeave(void* data, wl_data_device* data_device) {
  // While reading data, it could get OnLeave event. We don't handle OnLeave
  // event directly if |is_handling_dropped_data_| is set.
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  if (self->is_handling_dropped_data_) {
    self->is_leaving_ = true;
    return;
  }

  self->window_->OnDragLeave();
  self->window_ = nullptr;
  self->drag_offer_.reset();
  self->is_handling_dropped_data_ = false;
  self->is_leaving_ = false;
}

// static
void WaylandDataDevice::OnSelection(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer clipboard data available to paste.
  if (!offer) {
    self->selection_offer_.reset();

    // Clear Clipboard cache.
    self->connection_->SetClipboardData(std::string(), std::string());
    return;
  }

  DCHECK(self->new_offer_);
  self->selection_offer_ = std::move(self->new_offer_);

  self->selection_offer_->EnsureTextMimeTypeIfNeeded();
}

void WaylandDataDevice::SyncCallback(void* data,
                                     struct wl_callback* cb,
                                     uint32_t time) {
  WaylandDataDevice* data_device = static_cast<WaylandDataDevice*>(data);
  DCHECK(data_device);

  std::move(data_device->read_from_fd_closure_).Run();
  DCHECK(data_device->read_from_fd_closure_.is_null());
  data_device->sync_callback_.reset();
}

void WaylandDataDevice::CreateDragImage(const SkBitmap* bitmap) {
  DCHECK(bitmap);
  gfx::Size size(bitmap->width(), bitmap->height());

  if (size != icon_buffer_size_) {
    wl_buffer* buffer =
        wl::CreateSHMBuffer(size, shared_memory_.get(), connection_->shm());
    if (!buffer)
      return;

    buffer_.reset(buffer);
    icon_buffer_size_ = size;
  }
  wl::DrawBitmapToSHMB(icon_buffer_size_, *shared_memory_, *bitmap);

  icon_surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  wl_surface_attach(icon_surface_.get(), buffer_.get(), 0, 0);
  wl_surface_damage(icon_surface_.get(), 0, 0, icon_buffer_size_.width(),
                    icon_buffer_size_.height());
  wl_surface_commit(icon_surface_.get());
}

void WaylandDataDevice::OnDragDataReceived(const std::string& contents) {
  if (!contents.empty()) {
    AddToOSExchangeData(contents, unprocessed_mime_types_.front(),
                        received_data_.get());
  }

  unprocessed_mime_types_.erase(unprocessed_mime_types_.begin());

  // Read next data corresponding to the mime type.
  HandleNextMimeType();
}

void WaylandDataDevice::OnDragDataCollected() {
  unprocessed_mime_types_.clear();
  window_->OnDragDrop(std::move(received_data_));
  drag_offer_->FinishOffer();
  is_handling_dropped_data_ = false;

  HandleDeferredLeaveIfNeeded();
}

std::string WaylandDataDevice::SelectNextMimeType() {
  while (!unprocessed_mime_types_.empty()) {
    std::string& mime_type = unprocessed_mime_types_.front();
    if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUTF8) &&
        !received_data_->HasString()) {
      return mime_type;
    }
    // TODO(jkim): Handle other mime types as well.
    unprocessed_mime_types_.erase(unprocessed_mime_types_.begin());
  }
  return std::string();
}

void WaylandDataDevice::HandleNextMimeType() {
  std::string mime_type = SelectNextMimeType();
  if (!mime_type.empty()) {
    RequestDragData(mime_type,
                    base::BindOnce(&WaylandDataDevice::OnDragDataReceived,
                                   base::Unretained(this)));
  } else {
    OnDragDataCollected();
  }
}

void WaylandDataDevice::SetOperation(const int operation) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  if (operation & DragDropTypes::DRAG_COPY) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  }

  if (operation & DragDropTypes::DRAG_MOVE) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE)
      preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  }
  drag_offer_->SetAction(dnd_actions, preferred_action);
}

}  // namespace ui
