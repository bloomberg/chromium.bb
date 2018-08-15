// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_

#include <map>

#include "ui/gfx/buffer_types.h"

#include "base/files/file.h"
#include "base/message_loop/message_pump_libevent.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_data_device.h"
#include "ui/ozone/platform/wayland/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/wayland_data_source.h"
#include "ui/ozone/platform/wayland/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_output.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/wayland_touch.h"
#include "ui/ozone/public/clipboard_delegate.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"

struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace ui {

class WaylandWindow;

class WaylandConnection : public PlatformEventSource,
                          public ClipboardDelegate,
                          public ozone::mojom::WaylandConnection,
                          public base::MessagePumpLibevent::FdWatcher {
 public:
  WaylandConnection();
  ~WaylandConnection() override;

  bool Initialize();
  bool StartProcessingEvents();

  // ozone::mojom::WaylandConnection overrides:
  //
  // The overridden methods below are invoked by GPU.
  //
  // Called by the GPU and asks to import a wl_buffer based on a gbm file
  // descriptor.
  void CreateZwpLinuxDmabuf(base::File file,
                            uint32_t width,
                            uint32_t height,
                            const std::vector<uint32_t>& strides,
                            const std::vector<uint32_t>& offsets,
                            uint32_t format,
                            const std::vector<uint64_t>& modifiers,
                            uint32_t planes_count,
                            uint32_t buffer_id) override;
  // Called by the GPU to destroy the imported wl_buffer with a |buffer_id|.
  void DestroyZwpLinuxDmabuf(uint32_t buffer_id) override;
  // Called by the GPU and asks to attach a wl_buffer with a |buffer_id| to a
  // WaylandWindow with the specified |widget|.
  void ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                          uint32_t buffer_id) override;

  // Schedules a flush of the Wayland connection.
  void ScheduleFlush();

  wl_display* display() { return display_.get(); }
  wl_compositor* compositor() { return compositor_.get(); }
  wl_subcompositor* subcompositor() { return subcompositor_.get(); }
  wl_shm* shm() { return shm_.get(); }
  xdg_shell* shell() { return shell_.get(); }
  zxdg_shell_v6* shell_v6() { return shell_v6_.get(); }
  wl_seat* seat() { return seat_.get(); }
  wl_data_device* data_device() { return data_device_->data_device(); }
  zwp_linux_dmabuf_v1* zwp_linux_dmabuf() { return zwp_linux_dmabuf_.get(); }

  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget);
  WaylandWindow* GetCurrentFocusedWindow();
  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);

  int64_t get_next_display_id() { return next_display_id_++; }
  const std::vector<std::unique_ptr<WaylandOutput>>& GetOutputList() const;
  WaylandOutput* PrimaryOutput() const;

  void set_serial(uint32_t serial) { serial_ = serial; }
  uint32_t serial() { return serial_; }

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& location);

  int GetKeyboardModifiers();

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() { return pointer_.get(); }

  // Clipboard implementation.
  ClipboardDelegate* GetClipboardDelegate();
  void DataSourceCancelled();
  void SetClipboardData(const std::string& contents,
                        const std::string& mime_type);

  // ClipboardDelegate.
  void OfferClipboardData(
      const ClipboardDelegate::DataMap& data_map,
      ClipboardDelegate::OfferDataClosure callback) override;
  void RequestClipboardData(
      const std::string& mime_type,
      ClipboardDelegate::DataMap* data_map,
      ClipboardDelegate::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardDelegate::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner() override;

  // Returns bound pointer to own mojo interface.
  ozone::mojom::WaylandConnectionPtr BindInterface();

  std::vector<gfx::BufferFormat> GetSupportedBufferFormats();

  void SetTerminateGpuCallback(
      base::OnceCallback<void(std::string)> terminate_gpu_cb);

 private:
  void Flush();
  void DispatchUiEvent(Event* event);

  // PlatformEventSource
  void OnDispatcherListChanged() override;

  // base::MessagePumpLibevent::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Validates data sent by the GPU. If anything, terminates the gpu process.
  bool ValidateDataFromGpu(const base::File& file,
                           uint32_t width,
                           uint32_t height,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           uint32_t format,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           uint32_t buffer_id);

  // Terminates the GPU process on invalid data received
  void TerminateGpuProcess(std::string reason);

  // wl_registry_listener
  static void Global(void* data,
                     wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version);
  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // wl_seat_listener
  static void Capabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void Name(void* data, wl_seat* seat, const char* name);

  // zxdg_shell_v6_listener
  static void PingV6(void* data, zxdg_shell_v6* zxdg_shell_v6, uint32_t serial);

  // xdg_shell_listener
  static void Ping(void* data, xdg_shell* shell, uint32_t serial);

  // zwp_linux_dmabuf_v1_listener
  static void Modifiers(void* data,
                        struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                        uint32_t format,
                        uint32_t modifier_hi,
                        uint32_t modifier_lo);
  static void Format(void* data,
                     struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                     uint32_t format);

  static void CreateSucceeded(void* data,
                              struct zwp_linux_buffer_params_v1* params,
                              struct wl_buffer* new_buffer);
  static void CreateFailed(void* data,
                           struct zwp_linux_buffer_params_v1* params);

  std::map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  wl::Object<wl_display> display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<wl_seat> seat_;
  wl::Object<wl_shm> shm_;
  wl::Object<xdg_shell> shell_;
  wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf_;
  wl::Object<zxdg_shell_v6> shell_v6_;

  // Stores a wl_buffer and it's id provided by the GbmBuffer object on the
  // GPU process side.
  base::flat_map<uint32_t, wl::Object<wl_buffer>> buffers_;
  // A temporary params-to-buffer id map, which is used to identify which
  // id wl_buffer should be assigned when storing it in the |buffers_| map
  // during CreateSucceeded call.
  base::flat_map<struct zwp_linux_buffer_params_v1*, uint32_t>
      params_to_id_map_;
  // It might happen that GPU asks to swap buffers, when a wl_buffer hasn't
  // been created yet. Thus, store the request in a pending map. Once buffer
  // is created, it will be attached to requested WaylandWindow based on the
  // gfx::AcceleratedWidget.
  base::flat_map<uint32_t, gfx::AcceleratedWidget> pending_buffer_map_;

  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_;
  std::unique_ptr<WaylandDataDevice> data_device_;
  std::unique_ptr<WaylandDataSource> data_source_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandTouch> touch_;

  bool scheduled_flush_ = false;
  bool watching_ = false;
  base::MessagePumpLibevent::FdWatchController controller_;

  uint32_t serial_ = 0;

  int64_t next_display_id_ = 0;
  std::vector<std::unique_ptr<WaylandOutput>> output_list_;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  ClipboardDelegate::DataMap* data_map_ = nullptr;

  // Stores the callback to be invoked upon data reading from clipboard.
  RequestDataClosure read_clipboard_closure_;

  mojo::Binding<ozone::mojom::WaylandConnection> binding_;

  std::vector<gfx::BufferFormat> buffer_formats_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnection);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
