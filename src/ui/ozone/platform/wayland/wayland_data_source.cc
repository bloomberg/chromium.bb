// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_data_source.h"

#include "base/files/file_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

constexpr char kTextMimeType[] = "text/plain";
constexpr char kTextMimeTypeUtf8[] = "text/plain;charset=utf-8";

WaylandDataSource::WaylandDataSource(wl_data_source* data_source,
                                     WaylandConnection* connection)
    : data_source_(data_source), connection_(connection) {
  static const struct wl_data_source_listener kDataSourceListener = {
      WaylandDataSource::OnTarget,      WaylandDataSource::OnSend,
      WaylandDataSource::OnCancel,      WaylandDataSource::OnDnDDropPerformed,
      WaylandDataSource::OnDnDFinished, WaylandDataSource::OnAction};
  wl_data_source_add_listener(data_source, &kDataSourceListener, this);
}

WaylandDataSource::~WaylandDataSource() = default;

void WaylandDataSource::WriteToClipboard(
    const ClipboardDelegate::DataMap& data_map) {
  for (const auto& data : data_map) {
    wl_data_source_offer(data_source_.get(), data.first.c_str());
    if (strcmp(data.first.c_str(), kTextMimeType) == 0)
      wl_data_source_offer(data_source_.get(), kTextMimeTypeUtf8);
  }
  wl_data_device_set_selection(connection_->data_device(), data_source_.get(),
                               connection_->serial());

  wl_display_flush(connection_->display());
}

void WaylandDataSource::UpdataDataMap(
    const ClipboardDelegate::DataMap& data_map) {
  data_map_ = data_map;
}

void WaylandDataSource::Offer(const ui::OSExchangeData& data) {
  // TODO(jkim): Handle mime types based on data.
  std::vector<std::string> mime_types;
  mime_types.push_back(kTextMimeType);
  mime_types.push_back(kTextMimeTypeUtf8);

  source_window_ = connection_->GetCurrentFocusedWindow();
  for (auto& mime_type : mime_types)
    wl_data_source_offer(data_source_.get(), mime_type.data());
}

void WaylandDataSource::SetDragData(const DragDataMap& data_map) {
  DCHECK(drag_data_map_.empty());
  drag_data_map_ = data_map;
}

void WaylandDataSource::SetAction(int operation) {
  if (wl_data_source_get_version(data_source_.get()) >=
      WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION) {
    uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (operation & DragDropTypes::DRAG_COPY)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    if (operation & DragDropTypes::DRAG_MOVE)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    wl_data_source_set_actions(data_source_.get(), dnd_actions);
  }
}

// static
void WaylandDataSource::OnTarget(void* data,
                                 wl_data_source* source,
                                 const char* mime_type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandDataSource::OnSend(void* data,
                               wl_data_source* source,
                               const char* mime_type,
                               int32_t fd) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  std::string contents;
  if (self->source_window_) {
    // If |source_window_| is valid when OnSend() is called, it means that DnD
    // is working.
    self->GetDragData(mime_type, &contents);
  } else {
    base::Optional<std::vector<uint8_t>> mime_data;
    self->GetClipboardData(mime_type, &mime_data);
    if (!mime_data.has_value() && strcmp(mime_type, kTextMimeTypeUtf8) == 0)
      self->GetClipboardData(kTextMimeType, &mime_data);
    contents.assign(mime_data->begin(), mime_data->end());
  }
  bool result =
      base::WriteFileDescriptor(fd, contents.data(), contents.length());
  DCHECK(result);
  close(fd);
}

// static
void WaylandDataSource::OnCancel(void* data, wl_data_source* source) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  if (self->source_window_) {
    // If it has |source_window_|, it is in the middle of 'drag and drop'. it
    // cancels 'drag and drop'.
    self->connection_->FinishDragSession(self->dnd_action_,
                                         self->source_window_);
  } else {
    self->connection_->DataSourceCancelled();
  }
}

void WaylandDataSource::OnDnDDropPerformed(void* data, wl_data_source* source) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandDataSource::OnDnDFinished(void* data, wl_data_source* source) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  self->connection_->FinishDragSession(self->dnd_action_, self->source_window_);
}

void WaylandDataSource::OnAction(void* data,
                                 wl_data_source* source,
                                 uint32_t dnd_action) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  self->dnd_action_ = dnd_action;
}

void WaylandDataSource::GetClipboardData(
    const std::string& mime_type,
    base::Optional<std::vector<uint8_t>>* data) {
  auto it = data_map_.find(mime_type);
  if (it != data_map_.end()) {
    data->emplace(it->second);
    // TODO: return here?
    return;
  }
}

void WaylandDataSource::GetDragData(const std::string& mime_type,
                                    std::string* contents) {
  auto it = drag_data_map_.find(mime_type);
  if (it != drag_data_map_.end()) {
    *contents = it->second;
    return;
  }

  connection_->DeliverDragData(mime_type, contents);
}

}  // namespace ui
