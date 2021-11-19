// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_

#include <time.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/tablet_state.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

struct wl_cursor;
struct wl_event_queue;

namespace gfx {
class Point;
}

namespace wl {
class WaylandProxy;
}

namespace ui {

class DeviceHotplugEventObserver;
class OrgKdeKwinIdle;
class SurfaceAugmenter;
class WaylandBufferManagerHost;
class WaylandCursor;
class WaylandCursorBufferListener;
class WaylandDrm;
class WaylandEventSource;
class WaylandKeyboard;
class WaylandOutputManager;
class WaylandPointer;
class WaylandShm;
class WaylandTouch;
class WaylandZAuraShell;
class WaylandZcrCursorShapes;
class WaylandZwpPointerConstraints;
class WaylandZwpPointerGestures;
class WaylandZwpRelativePointerManager;
class WaylandZwpLinuxDmabuf;
class WaylandDataDeviceManager;
class WaylandCursorPosition;
class WaylandWindowDragController;
class GtkPrimarySelectionDeviceManager;
class GtkShell1;
class ZwpIdleInhibitManager;
class ZwpPrimarySelectionDeviceManager;
class XdgForeignWrapper;
class OverlayPrioritizer;

// These values are persisted to logs.  Entries should not be renumbered and
// numeric values should never be reused.
//
// Append new shells before kMaxValue and update LinuxWaylandShell
// in tools/metrics/histograms/enums.xml accordingly.
//
// See also tools/metrics/histograms/README.md#enum-histograms
enum class UMALinuxWaylandShell {
  kZauraShell = 0,
  kGtkShell1 = 1,
  kOrgKdePlasmaShell = 2,
  kXdgWmBase = 3,
  kXdgShellV6 = 4,
  kZwlrLayerShellV1 = 5,
  kMaxValue = kZwlrLayerShellV1,
};

void ReportShellUMA(UMALinuxWaylandShell shell);

class WaylandConnection {
 public:
  WaylandConnection();
  WaylandConnection(const WaylandConnection&) = delete;
  WaylandConnection& operator=(const WaylandConnection&) = delete;
  ~WaylandConnection();

  bool Initialize();

  void RegisterGlobalObjectFactory(const char* interface_name,
                                   wl::GlobalObjectFactory factory);

  // Schedules a flush of the Wayland connection.
  void ScheduleFlush();

  // Calls wl_display_roundtrip_queue. Might be required during initialization
  // of some objects that should block until they are initialized.
  void RoundTripQueue();

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Called by WaylandEventWatcher.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  // A correct display must be chosen when creating objects or calling
  // roundrips.  That is, all the methods that deal with polling, pulling event
  // queues, etc, must use original display. All the other methods that create
  // various wayland objects must use |display_wrapper_| so that the new objects
  // are associated with the correct event queue. Otherwise, they will use a
  // default event queue, which we do not use. See the comment below about the
  // |event_queue_|.
  wl_display* display() const { return display_.get(); }
  wl_display* display_wrapper() const {
    return reinterpret_cast<wl_display*>(wrapped_display_.get());
  }
  wl_compositor* compositor() const { return compositor_.get(); }
  // The server version of the compositor interface (might be higher than the
  // version binded).
  uint32_t compositor_version() const { return compositor_version_; }
  wl_subcompositor* subcompositor() const { return subcompositor_.get(); }
  wp_viewporter* viewporter() const { return viewporter_.get(); }
  zcr_alpha_compositing_v1* alpha_compositing() const {
    return alpha_compositing_.get();
  }
  xdg_wm_base* shell() const { return shell_.get(); }
  zxdg_shell_v6* shell_v6() const { return shell_v6_.get(); }
  wl_seat* seat() const { return seat_.get(); }
  wp_presentation* presentation() const { return presentation_.get(); }
  zwp_text_input_manager_v1* text_input_manager_v1() const {
    return text_input_manager_v1_.get();
  }
  zcr_text_input_extension_v1* text_input_extension_v1() const {
    return text_input_extension_v1_.get();
  }
  zwp_linux_explicit_synchronization_v1* linux_explicit_synchronization_v1()
      const {
    return linux_explicit_synchronization_.get();
  }
  zxdg_decoration_manager_v1* xdg_decoration_manager_v1() const {
    return xdg_decoration_manager_.get();
  }
  zcr_extended_drag_v1* extended_drag_v1() const {
    return extended_drag_v1_.get();
  }

  zxdg_output_manager_v1* xdg_output_manager_v1() const {
    return xdg_output_manager_.get();
  }

  void SetPlatformCursor(wl_cursor* cursor_data, int buffer_scale);

  void SetCursorBufferListener(WaylandCursorBufferListener* listener);

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& hotspot_in_dips,
                       int buffer_scale);

  WaylandEventSource* event_source() const { return event_source_.get(); }

  // Returns the current touch, which may be null.
  WaylandTouch* touch() const { return touch_.get(); }

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() const { return pointer_.get(); }

  // Returns the current keyboard, which may be null.
  WaylandKeyboard* keyboard() const { return keyboard_.get(); }

  WaylandClipboard* clipboard() const { return clipboard_.get(); }

  WaylandOutputManager* wayland_output_manager() const {
    return wayland_output_manager_.get();
  }

  // Returns the cursor position, which may be null.
  WaylandCursorPosition* wayland_cursor_position() const {
    return wayland_cursor_position_.get();
  }

  WaylandBufferManagerHost* buffer_manager_host() const {
    return buffer_manager_host_.get();
  }

  WaylandZAuraShell* zaura_shell() const { return zaura_shell_.get(); }

  WaylandZcrCursorShapes* zcr_cursor_shapes() const {
    return zcr_cursor_shapes_.get();
  }

  WaylandZwpLinuxDmabuf* zwp_dmabuf() const { return zwp_dmabuf_.get(); }

  WaylandDrm* drm() const { return drm_.get(); }

  WaylandShm* shm() const { return shm_.get(); }

  WaylandWindowManager* wayland_window_manager() {
    return &wayland_window_manager_;
  }

  WaylandDataDeviceManager* data_device_manager() const {
    return data_device_manager_.get();
  }

  GtkPrimarySelectionDeviceManager* gtk_primary_selection_device_manager()
      const {
    return gtk_primary_selection_device_manager_.get();
  }

  GtkShell1* gtk_shell1() { return gtk_shell1_.get(); }

  OrgKdeKwinIdle* org_kde_kwin_idle() { return org_kde_kwin_idle_.get(); }

  ZwpPrimarySelectionDeviceManager* zwp_primary_selection_device_manager()
      const {
    return zwp_primary_selection_device_manager_.get();
  }

  WaylandDataDragController* data_drag_controller() const {
    return data_drag_controller_.get();
  }

  WaylandWindowDragController* window_drag_controller() const {
    return window_drag_controller_.get();
  }

  WaylandZwpPointerConstraints* wayland_zwp_pointer_constraints() const {
    return wayland_zwp_pointer_constraints_.get();
  }

  WaylandZwpRelativePointerManager* wayland_zwp_relative_pointer_manager()
      const {
    return wayland_zwp_relative_pointer_manager_.get();
  }

  XdgForeignWrapper* xdg_foreign() const { return xdg_foreign_.get(); }

  ZwpIdleInhibitManager* zwp_idle_inhibit_manager() const {
    return zwp_idle_inhibit_manager_.get();
  }

  OverlayPrioritizer* overlay_prioritizer() const {
    return overlay_prioritizer_.get();
  }

  SurfaceAugmenter* surface_augmenter() const {
    return surface_augmenter_.get();
  }

  // Returns whether protocols that support setting window geometry are
  // available.
  bool SupportsSetWindowGeometry() const;

  // Returns true when dragging is entered or started.
  bool IsDragInProgress() const;

  // Creates a new wl_surface.
  wl::Object<wl_surface> CreateSurface();

  // base::TimeTicks::Now() in posix uses CLOCK_MONOTONIC, wp_presentation
  // timestamps are in clk_id sent in wp_presentation.clock_id event. This
  // converts wp_presentation timestamp to base::TimeTicks.
  // The approximation relies on presentation timestamp to be close to current
  // time. The further it is from current time and the bigger the speed
  // difference between the two clock domains, the bigger the conversion error.
  // Conversion error due to system load is biased and unbounded.
  base::TimeTicks ConvertPresentationTime(uint32_t tv_sec_hi,
                                          uint32_t tv_sec_lo,
                                          uint32_t tv_nsec);

  const std::vector<std::pair<std::string, uint32_t>>& available_globals()
      const {
    return available_globals_;
  }

  bool surface_submission_in_pixel_coordinates() const {
    return surface_submission_in_pixel_coordinates_;
  }

  void set_surface_submission_in_pixel_coordinates(bool enabled) {
    surface_submission_in_pixel_coordinates_ = enabled;
  }

  wl::SerialTracker& serial_tracker() { return serial_tracker_; }

  void set_tablet_layout_state(display::TabletState tablet_layout_state) {
    tablet_layout_state_ = tablet_layout_state;
  }
  bool GetTabletMode() {
    return tablet_layout_state_ == display::TabletState::kInTabletMode ||
           tablet_layout_state_ == display::TabletState::kEnteringTabletMode;
  }

 private:
  friend class WaylandConnectionTestApi;

  // All global Wayland objects are friends of the Wayland connection.
  // Conceptually, this is correct: globals are owned by the connection and are
  // accessed via it, so they are essentially parts of it.  Also this friendship
  // makes it possible to avoid exposing setters for all those global objects:
  // these setters would only be needed by the globals but would be visible to
  // everyone.
  friend class GtkPrimarySelectionDeviceManager;
  friend class GtkShell1;
  friend class OrgKdeKwinIdle;
  friend class OverlayPrioritizer;
  friend class SurfaceAugmenter;
  friend class WaylandDataDeviceManager;
  friend class WaylandDrm;
  friend class WaylandOutput;
  friend class WaylandShm;
  friend class WaylandZAuraShell;
  friend class WaylandZwpLinuxDmabuf;
  friend class WaylandZwpPointerConstraints;
  friend class WaylandZwpPointerGestures;
  friend class WaylandZwpRelativePointerManager;
  friend class WaylandZcrCursorShapes;
  friend class XdgForeignWrapper;
  friend class ZwpIdleInhibitManager;
  friend class ZwpPrimarySelectionDeviceManager;

  void Flush();
  void UpdateInputDevices(wl_seat* seat, uint32_t capabilities);

  // Initialize data-related objects if required protocol objects are already
  // in place, i.e: wl_seat and wl_data_device_manager.
  void CreateDataObjectsIfReady();

  // Creates WaylandKeyboard with the currently acquired protocol objects, if
  // possible. Returns true iff WaylandKeyboard was created.
  bool CreateKeyboard();

  DeviceHotplugEventObserver* GetHotplugEventObserver();

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

  // xdg_wm_base_listener
  static void Ping(void* data, xdg_wm_base* shell, uint32_t serial);

  // xdg_wm_base_listener
  static void ClockId(void* data, wp_presentation* shell_v6, uint32_t clk_id);

  base::flat_map<std::string, wl::GlobalObjectFactory> global_object_factories_;

  uint32_t compositor_version_ = 0;
  wl::Object<wl_display> display_;
  wl::Object<wl_proxy> wrapped_display_;
  wl::Object<wl_event_queue> event_queue_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<wl_seat> seat_;
  wl::Object<xdg_wm_base> shell_;
  wl::Object<zxdg_shell_v6> shell_v6_;
  wl::Object<wp_presentation> presentation_;
  wl::Object<wp_viewporter> viewporter_;
  wl::Object<zcr_alpha_compositing_v1> alpha_compositing_;
  wl::Object<zcr_keyboard_extension_v1> keyboard_extension_v1_;
  wl::Object<zwp_text_input_manager_v1> text_input_manager_v1_;
  wl::Object<zcr_text_input_extension_v1> text_input_extension_v1_;
  wl::Object<zwp_linux_explicit_synchronization_v1>
      linux_explicit_synchronization_;
  wl::Object<zxdg_decoration_manager_v1> xdg_decoration_manager_;
  wl::Object<zcr_extended_drag_v1> extended_drag_v1_;
  wl::Object<zxdg_output_manager_v1> xdg_output_manager_;

  // Manages Wayland windows.
  WaylandWindowManager wayland_window_manager_;

  // Event source instance. Must be declared before input objects so it
  // outlives them so thus being able to properly handle their destruction.
  std::unique_ptr<WaylandEventSource> event_source_;

  // Input device objects.
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandTouch> touch_;

  std::unique_ptr<WaylandCursor> cursor_;
  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_;
  std::unique_ptr<WaylandOutputManager> wayland_output_manager_;
  std::unique_ptr<WaylandCursorPosition> wayland_cursor_position_;
  std::unique_ptr<WaylandZAuraShell> zaura_shell_;
  std::unique_ptr<WaylandZcrCursorShapes> zcr_cursor_shapes_;
  std::unique_ptr<WaylandZwpPointerConstraints>
      wayland_zwp_pointer_constraints_;
  std::unique_ptr<WaylandZwpRelativePointerManager>
      wayland_zwp_relative_pointer_manager_;
  std::unique_ptr<WaylandZwpPointerGestures> wayland_zwp_pointer_gestures_;
  std::unique_ptr<WaylandZwpLinuxDmabuf> zwp_dmabuf_;
  std::unique_ptr<WaylandDrm> drm_;
  std::unique_ptr<WaylandShm> shm_;
  std::unique_ptr<WaylandBufferManagerHost> buffer_manager_host_;
  std::unique_ptr<XdgForeignWrapper> xdg_foreign_;
  std::unique_ptr<ZwpIdleInhibitManager> zwp_idle_inhibit_manager_;
  std::unique_ptr<OverlayPrioritizer> overlay_prioritizer_;
  std::unique_ptr<SurfaceAugmenter> surface_augmenter_;

  // Clipboard-related objects. |clipboard_| must be declared after all
  // DeviceManager instances it depends on, otherwise tests may crash with
  // UAFs while attempting to access already destroyed manager pointers.
  std::unique_ptr<GtkPrimarySelectionDeviceManager>
      gtk_primary_selection_device_manager_;
  std::unique_ptr<ZwpPrimarySelectionDeviceManager>
      zwp_primary_selection_device_manager_;
  std::unique_ptr<WaylandClipboard> clipboard_;

  std::unique_ptr<GtkShell1> gtk_shell1_;

  // Objects specific to KDE Plasma desktop environment.
  std::unique_ptr<OrgKdeKwinIdle> org_kde_kwin_idle_;

  std::unique_ptr<WaylandDataDragController> data_drag_controller_;
  std::unique_ptr<WaylandWindowDragController> window_drag_controller_;

  // Describes the clock domain that wp_presentation timestamps are in.
  uint32_t presentation_clk_id_ = CLOCK_MONOTONIC;

  // Helper class that lets input emulation access some data of objects
  // that Wayland holds. For example, wl_surface and others. It's only
  // created when platform window test config is set.
  std::unique_ptr<wl::WaylandProxy> wayland_proxy_;

  WaylandCursorBufferListener* listener_ = nullptr;

  // The current window table mode layout state.
  display::TabletState tablet_layout_state_ =
      display::TabletState::kInClamshellMode;

  bool scheduled_flush_ = false;

  // Surfaces are submitted in pixel coordinates. Their buffer scales are always
  // advertised to server as 1, and the scale via vp_viewporter won't be
  // applied. The server will be responsible to scale the buffers to the right
  // sizes.
  bool surface_submission_in_pixel_coordinates_ = false;

  wl::SerialTracker serial_tracker_;

  // Global Wayland interfaces available in the current session, with their
  // versions.
  std::vector<std::pair<std::string, uint32_t>> available_globals_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_
