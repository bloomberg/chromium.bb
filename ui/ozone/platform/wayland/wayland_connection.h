// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_

#include <map>

#include "base/files/file.h"
#include "base/message_loop/message_pump_libevent.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/wayland_data_device.h"
#include "ui/ozone/platform/wayland/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/wayland_data_source.h"
#include "ui/ozone/platform/wayland/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_output.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/wayland_touch.h"
#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class WaylandBufferManager;
class WaylandShmBufferManager;
class WaylandOutputManager;
class WaylandWindow;

// TODO: factor out PlatformClipboard to a separate class.
class WaylandConnection : public PlatformEventSource,
                          public PlatformClipboard,
                          public ozone::mojom::WaylandConnection,
                          public base::MessagePumpLibevent::FdWatcher {
 public:
  WaylandConnection();
  ~WaylandConnection() override;

  bool Initialize();
  bool StartProcessingEvents();

  // ozone::mojom::WaylandConnection overrides:
  //
  // These overridden methods below are invoked by the GPU when hardware
  // accelerated rendering is used.
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
                          uint32_t buffer_id,
                          const gfx::Rect& damage_region,
                          ScheduleBufferSwapCallback callback) override;
  // These overridden methods below are invoked by the GPU when hardware
  // accelerated rendering is not used. Check comments in the
  // ui/ozone/public/interfaces/wayland/wayland_connection.mojom.
  void CreateShmBufferForWidget(gfx::AcceleratedWidget widget,
                                base::File file,
                                uint64_t length,
                                const gfx::Size& size) override;
  void PresentShmBufferForWidget(gfx::AcceleratedWidget widget,
                                 const gfx::Rect& damage) override;
  void DestroyShmBuffer(gfx::AcceleratedWidget widget) override;

  // Schedules a flush of the Wayland connection.
  void ScheduleFlush();

  wl_display* display() const { return display_.get(); }
  wl_compositor* compositor() const { return compositor_.get(); }
  wl_subcompositor* subcompositor() const { return subcompositor_.get(); }
  wl_shm* shm() const { return shm_.get(); }
  xdg_shell* shell() const { return shell_.get(); }
  zxdg_shell_v6* shell_v6() const { return shell_v6_.get(); }
  wl_seat* seat() const { return seat_.get(); }
  wl_data_device* data_device() const { return data_device_->data_device(); }
  wp_presentation* presentation() const { return presentation_.get(); }
  zwp_text_input_manager_v1* text_input_manager_v1() const {
    return text_input_manager_v1_.get();
  }

  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget) const;
  WaylandWindow* GetWindowWithLargestBounds() const;
  WaylandWindow* GetCurrentFocusedWindow() const;
  WaylandWindow* GetCurrentKeyboardFocusedWindow() const;
  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);

  void set_serial(uint32_t serial) { serial_ = serial; }
  uint32_t serial() const { return serial_; }

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& location);

  int GetKeyboardModifiers() const;

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() const { return pointer_.get(); }

  WaylandDataSource* drag_data_source() const {
    return drag_data_source_.get();
  }

  WaylandOutputManager* wayland_output_manager() const {
    return wayland_output_manager_.get();
  }

  // Returns the cursor position, which may be null.
  WaylandCursorPosition* wayland_cursor_position() const {
    return wayland_cursor_position_.get();
  }

  WaylandBufferManager* buffer_manager() const { return buffer_manager_.get(); }

  // Clipboard implementation.
  PlatformClipboard* GetPlatformClipboard();
  void DataSourceCancelled();
  void SetClipboardData(const std::string& contents,
                        const std::string& mime_type);
  void UpdateClipboardSequenceNumber();

  // PlatformClipboard.
  void OfferClipboardData(
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner() override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

  // Returns bound pointer to own mojo interface.
  ozone::mojom::WaylandConnectionPtr BindInterface();

  std::vector<gfx::BufferFormat> GetSupportedBufferFormats();

  void SetTerminateGpuCallback(
      base::OnceCallback<void(std::string)> terminate_gpu_cb);

  // Starts drag with |data| to be delivered, |operation| supported by the
  // source side initiated the dragging.
  void StartDrag(const ui::OSExchangeData& data, int operation);
  // Finishes drag and drop session. It happens when WaylandDataSource gets
  // 'OnDnDFinished' or 'OnCancel', which means the drop is performed or
  // canceled on others.
  void FinishDragSession(uint32_t dnd_action, WaylandWindow* source_window);
  // Delivers the data owned by Chromium which initiates drag-and-drop. |buffer|
  // is an output parameter and it should be filled with the data corresponding
  // to mime_type.
  void DeliverDragData(const std::string& mime_type, std::string* buffer);
  // Requests the data to the platform when Chromium gets drag-and-drop started
  // by others. Once reading the data from platform is done, |callback| should
  // be called with the data.
  void RequestDragData(const std::string& mime_type,
                       base::OnceCallback<void(const std::string&)> callback);

  // Returns true when dragging is entered or started.
  bool IsDragInProgress();

  // Resets flags and keyboard modifiers.
  //
  // This method is specially handy for cases when the WaylandPointer state is
  // modified by a POINTER_DOWN event, but the respective POINTER_UP event is
  // not delivered.
  void ResetPointerFlags();

 private:
  // WaylandInputMethodContextFactory needs access to DispatchUiEvent
  friend class WaylandInputMethodContextFactory;

  void Flush();
  void DispatchUiEvent(Event* event);

  // PlatformEventSource
  void OnDispatcherListChanged() override;

  // base::MessagePumpLibevent::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Terminates the GPU process on invalid data received
  void TerminateGpuProcess(std::string reason);

  // Make sure data device is properly initialized
  void EnsureDataDevice();

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

  std::map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  wl::Object<wl_display> display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<wl_seat> seat_;
  wl::Object<wl_shm> shm_;
  wl::Object<xdg_shell> shell_;
  wl::Object<zxdg_shell_v6> shell_v6_;
  wl::Object<wp_presentation> presentation_;
  wl::Object<zwp_text_input_manager_v1> text_input_manager_v1_;

  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_;
  std::unique_ptr<WaylandDataDevice> data_device_;
  std::unique_ptr<WaylandDataSource> data_source_;
  std::unique_ptr<WaylandDataSource> drag_data_source_;
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandOutputManager> wayland_output_manager_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandTouch> touch_;
  std::unique_ptr<WaylandCursorPosition> wayland_cursor_position_;
  std::unique_ptr<WaylandShmBufferManager> shm_buffer_manager_;

  // Objects that are using when GPU runs in own process.
  std::unique_ptr<WaylandBufferManager> buffer_manager_;

  bool scheduled_flush_ = false;
  bool watching_ = false;
  base::MessagePumpLibevent::FdWatchController controller_;

  uint32_t serial_ = 0;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  PlatformClipboard::DataMap* data_map_ = nullptr;

  // Notifies whenever clipboard sequence number is changed. Can be empty if not
  // set.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // Stores the callback to be invoked upon data reading from clipboard.
  RequestDataClosure read_clipboard_closure_;

  mojo::Binding<ozone::mojom::WaylandConnection> binding_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnection);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
