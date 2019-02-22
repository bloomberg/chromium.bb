// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_

#include <wayland-client.h>
#include <list>
#include <string>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

class SkBitmap;

namespace base {
class SharedMemory;
}

namespace ui {

class OSExchangeData;
class WaylandDataOffer;
class WaylandConnection;
class WaylandWindow;

// This class provides access to inter-client data transfer mechanisms
// such as copy-and-paste and drag-and-drop mechanisms.
class WaylandDataDevice {
 public:
  WaylandDataDevice(WaylandConnection* connection, wl_data_device* data_device);
  ~WaylandDataDevice();

  void RequestSelectionData(const std::string& mime_type);

  // Requests the data to the platform when Chromium gets drag-and-drop started
  // by others. Once reading the data from platform is done, |callback| should
  // be called with the data.
  void RequestDragData(const std::string& mime_type,
                       base::OnceCallback<void(const std::string&)> callback);
  // Delivers the data owned by Chromium which initiates drag-and-drop. |buffer|
  // is an output parameter and it should be filled with the data corresponding
  // to mime_type.
  void DeliverDragData(const std::string& mime_type, std::string* buffer);
  // Starts drag with |data| to be delivered, |operation| supported by the
  // source side initiated the dragging.
  void StartDrag(const wl_data_source& data_source,
                 const ui::OSExchangeData& data);
  // Resets |source_data_| when the dragging is finished.
  void ResetSourceData();

  std::vector<std::string> GetAvailableMimeTypes();

  wl_data_device* data_device() const { return data_device_.get(); }

 private:
  void ReadClipboardDataFromFD(base::ScopedFD fd, const std::string& mime_type);

  void ReadDragDataFromFD(
      base::ScopedFD fd,
      base::OnceCallback<void(const std::string&)> callback);

  // Registers display sync callback. Once it's called, it's reset.
  void RegisterSyncCallback();

  // Helper function to read data from fd.
  void ReadDataFromFD(base::ScopedFD fd, std::string* contents);

  // If OnLeave event occurs while it's reading drag data, it defers handling
  // it. Once reading data is completed, it's handled.
  void HandleDeferredLeaveIfNeeded();

  // wl_data_device_listener callbacks
  static void OnDataOffer(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  static void OnEnter(void* data,
                      wl_data_device* data_device,
                      uint32_t serial,
                      wl_surface* surface,
                      wl_fixed_t x,
                      wl_fixed_t y,
                      wl_data_offer* offer);

  static void OnMotion(void* data,
                       struct wl_data_device* data_device,
                       uint32_t time,
                       wl_fixed_t x,
                       wl_fixed_t y);

  static void OnDrop(void* data, struct wl_data_device* data_device);

  static void OnLeave(void* data, struct wl_data_device* data_device);

  // Called by the compositor when the window gets pointer or keyboard focus,
  // or clipboard content changes behind the scenes.
  //
  // https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_data_device
  static void OnSelection(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  static void SyncCallback(void* data, struct wl_callback* cb, uint32_t time);

  bool CreateSHMBuffer(const gfx::Size& size);
  void CreateDragImage(const SkBitmap* bitmap);

  void OnDragDataReceived(const std::string& contents);
  void OnDragDataCollected();

  // Returns the next MIME type to be received from the source process, or an
  // empty string if there are no more interesting MIME types left to process.
  std::string SelectNextMimeType();
  // If it has |unprocessed_mime_types_|, it takes the mime type in front and
  // requests the data corresponding to the mime type to wayland.
  void HandleNextMimeType();

  // Set drag operation decided by client.
  void SetOperation(const int operation);

  // The wl_data_device wrapped by this WaylandDataDevice.
  wl::Object<wl_data_device> data_device_;

  // Used to call out to WaylandConnection once clipboard data
  // has been successfully read.
  WaylandConnection* connection_ = nullptr;

  // There are two separate data offers at a time, the drag offer and the
  // selection offer, each with independent lifetimes. When we receive a new
  // offer, it is not immediately possible to know whether the new offer is the
  // drag offer or the selection offer. This variable is used to store ownership
  // of new data offers temporarily until its identity becomes known.
  std::unique_ptr<WaylandDataOffer> new_offer_;

  // Offer that holds the most-recent clipboard selection, or null if no
  // clipboard data is available.
  std::unique_ptr<WaylandDataOffer> selection_offer_;

  // Offer to receive data from another process via drag-and-drop, or null if no
  // drag-and-drop from another process is in progress.
  std::unique_ptr<WaylandDataOffer> drag_offer_;

  WaylandWindow* window_ = nullptr;

  // Make sure server has written data on the pipe, before block on read().
  static const wl_callback_listener callback_listener_;
  base::OnceClosure read_from_fd_closure_;
  wl::Object<wl_callback> sync_callback_;

  bool is_handling_dropped_data_ = false;
  bool is_leaving_ = false;

  std::unique_ptr<base::SharedMemory> shared_memory_;

  wl::Object<wl_buffer> buffer_;
  wl::Object<wl_surface> icon_surface_;
  gfx::Size icon_buffer_size_;

  // Mime types to be handled.
  std::list<std::string> unprocessed_mime_types_;

  // The data delivered from Wayland
  std::unique_ptr<ui::OSExchangeData> received_data_;

  // When dragging is started from Chromium, |source_data_| is forwarded to
  // Wayland when they are ready to get the data.
  std::unique_ptr<ui::OSExchangeData> source_data_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_
