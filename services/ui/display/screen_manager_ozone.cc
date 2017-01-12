// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/screen_manager_ozone.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/system/devicemode.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/manager/chromeos/display_change_observer.h"
#include "ui/display/manager/chromeos/touch_transform_controller.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/fake_display_controller.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {
namespace {

// Needed for DisplayConfigurator::ForceInitialConfigure.
const SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

// Recursively swaps the displays in a DisplayLayout to change the primary
// display but keep the same relative display layout.
// TODO(kylechar): This is copied from WindowTreeHostManager. The concept of
// getting the same relative display layout with a different primary display id
// should become a function on DisplayLayout itself to avoid reimplementing it
// here.
void SwapRecursive(const std::map<int64_t, DisplayPlacement*>& id_to_placement,
                   int64_t current_primary_id,
                   int64_t display_id) {
  if (display_id == current_primary_id)
    return;

  DCHECK(id_to_placement.count(display_id));
  DisplayPlacement* placement = id_to_placement.at(display_id);
  DCHECK(placement);
  SwapRecursive(id_to_placement, current_primary_id,
                placement->parent_display_id);
  placement->Swap();
}

}  // namespace

// static
std::unique_ptr<ScreenManager> ScreenManager::Create() {
  return base::MakeUnique<ScreenManagerOzone>();
}

ScreenManagerOzone::ScreenManagerOzone() {}

ScreenManagerOzone::~ScreenManagerOzone() {
  // We are shutting down and don't want to make anymore display changes.
  fake_display_controller_ = nullptr;

  touch_transform_controller_.reset();

  if (display_manager_)
    display_manager_->RemoveObserver(this);

  if (display_change_observer_) {
    display_configurator_.RemoveObserver(display_change_observer_.get());
    display_change_observer_.reset();
  }

  if (display_manager_)
    display_manager_.reset();
}

void ScreenManagerOzone::SetPrimaryDisplayId(int64_t display_id) {
  DCHECK_NE(kInvalidDisplayId, display_id);
  if (primary_display_id_ == display_id)
    return;

  const Display& new_primary_display =
      display_manager_->GetDisplayForId(display_id);
  if (!new_primary_display.is_valid()) {
    LOG(ERROR) << "Invalid or non-existent display is requested:"
               << new_primary_display.ToString();
    return;
  }

  int64_t old_primary_display_id = primary_display_id_;

  const DisplayLayout& layout = display_manager_->GetCurrentDisplayLayout();
  // The requested primary id can be same as one in the stored layout
  // when the primary id is set after new displays are connected.
  // Only update the layout if it is requested to swap primary display.
  if (layout.primary_id != new_primary_display.id()) {
    std::unique_ptr<DisplayLayout> swapped_layout(layout.Copy());

    std::map<int64_t, DisplayPlacement*> id_to_placement;
    for (auto& placement : swapped_layout->placement_list)
      id_to_placement[placement.display_id] = &placement;
    SwapRecursive(id_to_placement, primary_display_id_,
                  new_primary_display.id());

    std::sort(swapped_layout->placement_list.begin(),
              swapped_layout->placement_list.end(),
              [](const DisplayPlacement& d1, const DisplayPlacement& d2) {
                return d1.display_id < d2.display_id;
              });

    swapped_layout->primary_id = new_primary_display.id();
    DisplayIdList list = display_manager_->GetCurrentDisplayIdList();
    display_manager_->layout_store()->RegisterLayoutForDisplayIdList(
        list, std::move(swapped_layout));
  }

  primary_display_id_ = new_primary_display.id();
  screen_->display_list().UpdateDisplay(new_primary_display,
                                        DisplayList::Type::PRIMARY);

  // Force updating display bounds for new primary display.
  display_manager_->set_force_bounds_changed(true);
  display_manager_->UpdateDisplays();
  display_manager_->set_force_bounds_changed(false);

  DVLOG(1) << "Primary display changed from " << old_primary_display_id
           << " to " << primary_display_id_;
  delegate_->OnPrimaryDisplayChanged(primary_display_id_);
}

void ScreenManagerOzone::AddInterfaces(
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::DisplayController>(this);
  registry->AddInterface<mojom::TestDisplayController>(this);
}

void ScreenManagerOzone::Init(ScreenManagerDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  // Tests may inject a NativeDisplayDelegate, otherwise get it from
  // OzonePlatform.
  if (!native_display_delegate_) {
    native_display_delegate_ =
        ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
  }

  // The FakeDisplayController gives us a way to make the NativeDisplayDelegate
  // pretend something display related has happened.
  if (!chromeos::IsRunningAsSystemCompositor()) {
    fake_display_controller_ =
        native_display_delegate_->GetFakeDisplayController();
  }

  // Create a new Screen instance.
  std::unique_ptr<ScreenBase> screen = base::MakeUnique<ScreenBase>();
  Screen::SetScreenInstance(screen.get());
  screen_ = screen.get();

  // Configure display manager. ScreenManager acts as an observer to find out
  // display changes and as a delegate to find out when changes start/stop.
  display_manager_ = base::MakeUnique<DisplayManager>(std::move(screen));
  display_manager_->set_configure_displays(true);
  display_manager_->AddObserver(this);
  display_manager_->set_delegate(this);

  // DisplayChangeObserver observes DisplayConfigurator and sends updates to
  // DisplayManager.
  display_change_observer_ = base::MakeUnique<DisplayChangeObserver>(
      &display_configurator_, display_manager_.get());

  // We want display configuration to happen even off device to keep the control
  // flow similar.
  display_configurator_.set_configure_display(true);
  display_configurator_.AddObserver(display_change_observer_.get());
  display_configurator_.set_state_controller(display_change_observer_.get());
  display_configurator_.set_mirroring_controller(display_manager_.get());

  // Perform initial configuration.
  display_configurator_.Init(std::move(native_display_delegate_), false);
  display_configurator_.ForceInitialConfigure(kChromeOsBootColor);

  touch_transform_controller_ = base::MakeUnique<TouchTransformController>(
      &display_configurator_, display_manager_.get());
}

void ScreenManagerOzone::RequestCloseDisplay(int64_t display_id) {
  if (!fake_display_controller_)
    return;

  // Tell the NDD to remove the display. ScreenManager will get an update
  // that the display configuration has changed and the display will be gone.
  fake_display_controller_->RemoveDisplay(display_id);
}

int64_t ScreenManagerOzone::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

void ScreenManagerOzone::ToggleAddRemoveDisplay() {
  if (!fake_display_controller_)
    return;
  DVLOG(1) << "ToggleAddRemoveDisplay";

  int num_displays = display_manager_->GetNumDisplays();
  if (num_displays == 1) {
    const gfx::Size& pixel_size =
        display_manager_->GetDisplayInfo(display_manager_->GetDisplayAt(0).id())
            .bounds_in_native()
            .size();
    fake_display_controller_->AddDisplay(pixel_size);
  } else if (num_displays > 1) {
    DisplayIdList displays = display_manager_->GetCurrentDisplayIdList();
    fake_display_controller_->RemoveDisplay(displays.back());
  }
}

void ScreenManagerOzone::ToggleDisplayResolution() {
  if (primary_display_id_ == kInvalidDisplayId)
    return;

  // Internal displays don't have alternate resolutions.
  if (Display::HasInternalDisplay() &&
      primary_display_id_ == Display::InternalDisplayId())
    return;

  DVLOG(1) << "ToggleDisplayResolution";

  const ManagedDisplayInfo& info =
      display_manager_->GetDisplayInfo(primary_display_id_);
  scoped_refptr<ManagedDisplayMode> mode =
      GetDisplayModeForNextResolution(info, true);

  // Loop back to first mode from last.
  if (mode->size() == info.bounds_in_native().size())
    mode = info.display_modes()[0];

  // Set mode only if it's different from current.
  if (mode->size() != info.bounds_in_native().size())
    display_manager_->SetDisplayMode(primary_display_id_, mode);
}

void ScreenManagerOzone::IncreaseInternalDisplayZoom() {
  if (Display::HasInternalDisplay())
    display_manager_->ZoomInternalDisplay(false);
}

void ScreenManagerOzone::DecreaseInternalDisplayZoom() {
  if (Display::HasInternalDisplay())
    display_manager_->ZoomInternalDisplay(true);
}

void ScreenManagerOzone::ResetInternalDisplayZoom() {
  if (Display::HasInternalDisplay())
    display_manager_->ResetInternalDisplayZoom();
}

void ScreenManagerOzone::RotateCurrentDisplayCW() {
  NOTIMPLEMENTED();
}

void ScreenManagerOzone::SwapPrimaryDisplay() {
  // Can't swap if there is only 1 display and swapping isn't supported for 3 or
  // more displays.
  if (display_manager_->GetNumDisplays() != 2)
    return;

  DVLOG(1) << "SwapPrimaryDisplay()";

  DisplayIdList display_ids = display_manager_->GetCurrentDisplayIdList();

  // Find the next primary display.
  if (primary_display_id_ == display_ids[0])
    SetPrimaryDisplayId(display_ids[1]);
  else
    SetPrimaryDisplayId(display_ids[0]);
}

void ScreenManagerOzone::ToggleMirrorMode() {
  NOTIMPLEMENTED();
}

void ScreenManagerOzone::SetDisplayWorkArea(int64_t display_id,
                                            const gfx::Size& size,
                                            const gfx::Insets& insets) {
  // TODO(kylechar): Check the size of the display matches the current size.
  display_manager_->UpdateWorkAreaOfDisplay(display_id, insets);
}

void ScreenManagerOzone::OnDisplayAdded(const Display& display) {
  ViewportMetrics metrics = GetViewportMetricsForDisplay(display);
  DVLOG(1) << "OnDisplayAdded: " << display.ToString() << "\n  "
           << metrics.ToString();
  screen_->display_list().AddDisplay(display, DisplayList::Type::NOT_PRIMARY);
  delegate_->OnDisplayAdded(display.id(), metrics);
}

void ScreenManagerOzone::OnDisplayRemoved(const Display& display) {
  // TODO(kylechar): If we're removing the primary display we need to first set
  // a new primary display. This will crash until then.

  DVLOG(1) << "OnDisplayRemoved: " << display.ToString();
  screen_->display_list().RemoveDisplay(display.id());
  delegate_->OnDisplayRemoved(display.id());
}

void ScreenManagerOzone::OnDisplayMetricsChanged(const Display& display,
                                                 uint32_t changed_metrics) {
  ViewportMetrics metrics = GetViewportMetricsForDisplay(display);
  DVLOG(1) << "OnDisplayModified: " << display.ToString() << "\n  "
           << metrics.ToString();
  screen_->display_list().UpdateDisplay(display);
  delegate_->OnDisplayModified(display.id(), metrics);
}

ViewportMetrics ScreenManagerOzone::GetViewportMetricsForDisplay(
    const Display& display) {
  const ManagedDisplayInfo& managed_info =
      display_manager_->GetDisplayInfo(display.id());

  ViewportMetrics metrics;
  metrics.bounds = display.bounds();
  metrics.work_area = display.work_area();
  metrics.pixel_size = managed_info.bounds_in_native().size();
  metrics.rotation = display.rotation();
  metrics.touch_support = display.touch_support();
  metrics.device_scale_factor = display.device_scale_factor();
  metrics.ui_scale_factor = managed_info.configured_ui_scale();

  return metrics;
}

void ScreenManagerOzone::CreateOrUpdateMirroringDisplay(
    const DisplayInfoList& display_info_list) {
  NOTIMPLEMENTED();
}

void ScreenManagerOzone::CloseMirroringDisplayIfNotNecessary() {
  NOTIMPLEMENTED();
}

void ScreenManagerOzone::PreDisplayConfigurationChange(bool clear_focus) {
  DVLOG(1) << "PreDisplayConfigurationChange";
}

void ScreenManagerOzone::PostDisplayConfigurationChange(
    bool must_clear_window) {
  // Set primary display if not set yet.
  if (primary_display_id_ == kInvalidDisplayId) {
    const Display& primary_display =
        display_manager_->GetPrimaryDisplayCandidate();
    if (primary_display.is_valid()) {
      primary_display_id_ = primary_display.id();
      DVLOG(1) << "Set primary display to " << primary_display_id_;
      screen_->display_list().UpdateDisplay(primary_display,
                                            DisplayList::Type::PRIMARY);
      delegate_->OnPrimaryDisplayChanged(primary_display_id_);
    }
  }

  touch_transform_controller_->UpdateTouchTransforms();

  DVLOG(1) << "PostDisplayConfigurationChange";
}

DisplayConfigurator* ScreenManagerOzone::display_configurator() {
  return &display_configurator_;
}

void ScreenManagerOzone::Create(
    const service_manager::Identity& remote_identity,
    mojom::DisplayControllerRequest request) {
  controller_bindings_.AddBinding(this, std::move(request));
}

void ScreenManagerOzone::Create(
    const service_manager::Identity& remote_identity,
    mojom::TestDisplayControllerRequest request) {
  test_bindings_.AddBinding(this, std::move(request));
}

}  // namespace display
