// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_tree_tracker.h"

#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/window_properties.h"
#include "extensions/browser/event_router.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

ArcAccessibilityTreeTracker::TreeKey KeyForInputMethod() {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kInputMethod,
          kNoTaskId,
          {} /* notification_key */};
}

ArcAccessibilityTreeTracker::TreeKey KeyForNotification(
    std::string notification_key) {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kNotificationKey, kNoTaskId,
          std::move(notification_key)};
}

ArcAccessibilityTreeTracker::TreeKey KeyForTaskId(int32_t task_id) {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kTaskId,
          task_id,
          {} /* notification_key */};
}

bool ShouldTrackWindow(aura::Window* window) {
  return arc::IsArcOrGhostWindow(window);
}

void SetChildAxTreeIDForWindow(aura::Window* window,
                               const ui::AXTreeID& treeID) {
  DCHECK(window);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget)
    return;

  static_cast<exo::ShellSurfaceBase*>(widget->widget_delegate())
      ->SetChildAxTreeId(treeID);
}

void UpdateTreeIdOfNotificationSurface(const std::string& notification_key,
                                       const ui::AXTreeID& tree_id) {
  auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
  if (!surface_manager)
    return;

  ash::ArcNotificationSurface* surface =
      surface_manager->GetArcSurface(notification_key);
  if (!surface)
    return;

  surface->SetAXTreeId(tree_id);

  if (surface->IsAttached()) {
    // Dispatch ax::mojom::Event::kChildrenChanged to force AXNodeData of the
    // notification updated.
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, false);
  }
}

extensions::api::accessibility_private::SetNativeChromeVoxResponse
FromMojomResponseToAutomationResponse(
    arc::mojom::SetNativeChromeVoxResponse response) {
  switch (response) {
    case arc::mojom::SetNativeChromeVoxResponse::SUCCESS:
      return extensions::api::accessibility_private::
          SetNativeChromeVoxResponse::SET_NATIVE_CHROME_VOX_RESPONSE_SUCCESS;
    case arc::mojom::SetNativeChromeVoxResponse::TALKBACK_NOT_INSTALLED:
      return extensions::api::accessibility_private::
          SetNativeChromeVoxResponse::
              SET_NATIVE_CHROME_VOX_RESPONSE_TALKBACKNOTINSTALLED;
    case arc::mojom::SetNativeChromeVoxResponse::WINDOW_NOT_FOUND:
      return extensions::api::accessibility_private::
          SetNativeChromeVoxResponse::
              SET_NATIVE_CHROME_VOX_RESPONSE_WINDOWNOTFOUND;
    case arc::mojom::SetNativeChromeVoxResponse::FAILURE:
      return extensions::api::accessibility_private::
          SetNativeChromeVoxResponse::SET_NATIVE_CHROME_VOX_RESPONSE_FAILURE;
  }
}

}  // namespace

class ArcAccessibilityTreeTracker::FocusChangeObserver
    : public aura::client::FocusChangeObserver {
 public:
  explicit FocusChangeObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->AddFocusObserver(this);
  }
  ~FocusChangeObserver() override {
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->RemoveFocusObserver(this);
  }

  void OnWindowFocused(aura::Window* original_gained_focus,
                       aura::Window* original_lost_focus) override {
    aura::Window* gained_focus = FindArcOrGhostWindow(original_gained_focus);
    aura::Window* lost_focus = FindArcOrGhostWindow(original_lost_focus);
    if (gained_focus == lost_focus)
      return;

    owner_->OnWindowFocused(gained_focus, lost_focus);
  }

 private:
  ArcAccessibilityTreeTracker* owner_;
  // Different from other inner classes, this doesn't use ScopedObservation
  // because exo::WMHelper can be destroyed earlier than this class.
};

class ArcAccessibilityTreeTracker::WindowsObserver
    : public aura::WindowObserver {
 public:
  explicit WindowsObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {}

  void Observe(aura::Window* window) {
    if (window_observations_.IsObservingSource(window))
      return;

    window_observations_.AddObservation(window);
  }

  void Reset() { window_observations_.RemoveAllObservations(); }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != exo::kApplicationIdKey &&
        key != ash::kClientAccessibilityIdKey) {
      return;
    }
    owner_->UpdateWindowIdMapping(window);
  }

  void OnWindowDestroying(aura::Window* window) override {
    if (window_observations_.IsObservingSource(window))
      window_observations_.RemoveObservation(window);
    owner_->OnWindowDestroying(window);
  }

 private:
  ArcAccessibilityTreeTracker* owner_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

class ArcAccessibilityTreeTracker::ArcInputMethodManagerServiceObserver
    : public ArcInputMethodManagerService::Observer {
 public:
  ArcInputMethodManagerServiceObserver(ArcAccessibilityTreeTracker* owner,
                                       Profile* const profile)
      : owner_(owner) {
    auto* arc_ime_service =
        ArcInputMethodManagerService::GetForBrowserContext(profile);
    if (arc_ime_service)
      arc_imms_observation_.Observe(arc_ime_service);
  }

  void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) override {
    owner_->OnAndroidVirtualKeyboardVisibilityChanged(visible);
  }

 private:
  base::ScopedObservation<ArcInputMethodManagerService,
                          ArcInputMethodManagerService::Observer>
      arc_imms_observation_{this};
  ArcAccessibilityTreeTracker* owner_;
};

class ArcAccessibilityTreeTracker::MojoConnectionObserver
    : public ConnectionObserver<mojom::AccessibilityHelperInstance> {
 public:
  MojoConnectionObserver(ArcAccessibilityTreeTracker* owner,
                         ArcBridgeService* const arc_bridge_service)
      : owner_(owner) {
    helper_instance_connection_observation_.Observe(
        arc_bridge_service->accessibility_helper());
  }

  void OnConnectionReady() override {
    owner_->notification_surface_observer_ =
        std::make_unique<ArcNotificationSurfaceManagerObserver>(owner_);
  }

  void OnConnectionClosed() override {
    owner_->notification_surface_observer_.reset();
  }

 private:
  base::ScopedObservation<
      ConnectionHolder<mojom::AccessibilityHelperInstance,
                       mojom::AccessibilityHelperHost>,
      ConnectionObserver<mojom::AccessibilityHelperInstance>>
      helper_instance_connection_observation_{this};
  ArcAccessibilityTreeTracker* owner_;
};

class ArcAccessibilityTreeTracker::ArcNotificationSurfaceManagerObserver
    : public ash::ArcNotificationSurfaceManager::Observer {
 public:
  explicit ArcNotificationSurfaceManagerObserver(
      ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {
    auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
    if (surface_manager)
      arc_notification_observation_.Observe(surface_manager);
  }

  void OnNotificationSurfaceAdded(
      ash::ArcNotificationSurface* surface) override {
    owner_->OnNotificationSurfaceAdded(surface);
  }

  void OnNotificationSurfaceRemoved(
      ash::ArcNotificationSurface* surface) override {
    owner_->OnNotificationSurfaceRemoved(surface);
  }

 private:
  base::ScopedObservation<ash::ArcNotificationSurfaceManager,
                          ash::ArcNotificationSurfaceManager::Observer>
      arc_notification_observation_{this};
  ArcAccessibilityTreeTracker* owner_;
};

ArcAccessibilityTreeTracker::ArcAccessibilityTreeTracker(
    AXTreeSourceArc::Delegate* tree_source_delegate,
    Profile* const profile,
    const AccessibilityHelperInstanceRemoteProxy& accessibility_helper_instance,
    ArcBridgeService* const arc_bridge_service)
    : profile_(profile),
      tree_source_delegate_(tree_source_delegate),
      accessibility_helper_instance_(accessibility_helper_instance),
      windows_observer_(std::make_unique<WindowsObserver>(this)),
      input_manager_service_observer_(
          std::make_unique<ArcInputMethodManagerServiceObserver>(this,
                                                                 profile)),
      connection_observer_(
          std::make_unique<MojoConnectionObserver>(this, arc_bridge_service)) {}

ArcAccessibilityTreeTracker::~ArcAccessibilityTreeTracker() = default;

void ArcAccessibilityTreeTracker::OnWindowInitialized(aura::Window* window) {
  if (ShouldTrackWindow(window))
    TrackWindow(window);
}

void ArcAccessibilityTreeTracker::OnWindowFocused(aura::Window* gained_focus,
                                                  aura::Window* lost_focus) {
  UpdateWindowProperties(gained_focus);

  // Transitioning with ARC and non-ARC window may need to dispatch
  // ToggleNativeChromeVoxArcSupport event.
  //  - When non-ChromeVox ARC window becomes inactive, dispatch |true|.
  //  - When non-ChromeVox ARC window becomes active, dispatch |false|.
  bool lost_arc = ShouldTrackWindow(lost_focus);
  bool gained_arc = ShouldTrackWindow(gained_focus);
  bool talkback_enabled = !native_chromevox_enabled_;
  if (talkback_enabled && lost_arc != gained_arc)
    DispatchCustomSpokenFeedbackToggled(gained_arc);
}

void ArcAccessibilityTreeTracker::OnWindowDestroying(aura::Window* window) {
  const auto& task_id_opt = arc::GetWindowTaskId(window);
  if (!task_id_opt.has_value())
    return;

  int32_t task_id = task_id_opt.value();
  task_id_to_window_.erase(task_id);
  trees_.erase(KeyForTaskId(task_id));
  base::EraseIf(window_id_to_task_id_,
                [task_id](auto it) { return it.second == task_id; });
}

void ArcAccessibilityTreeTracker::Shutdown() {
  input_manager_service_observer_.reset();
}

void ArcAccessibilityTreeTracker::OnEnabledFeatureChanged(
    arc::mojom::AccessibilityFilterType filter_type) {
  if (filter_type_ == filter_type)
    return;

  filter_type_ = filter_type;

  if (filter_type_ == arc::mojom::AccessibilityFilterType::ALL) {
    focus_change_observer_ = std::make_unique<FocusChangeObserver>(this);
    StartTrackingWindows();
  } else {
    // No longer need to track windows and trees.
    trees_.clear();
    window_id_to_task_id_.clear();
    focus_change_observer_.reset();

    DCHECK(aura::Env::HasInstance());
    env_observation_.Reset();
    windows_observer_->Reset();
  }
}

bool ArcAccessibilityTreeTracker::EnableTree(const ui::AXTreeID& tree_id) {
  AXTreeSourceArc* tree_source = GetFromTreeId(tree_id);
  if (!tree_source || !tree_source->window())
    return false;

  arc::mojom::AccessibilityWindowKeyPtr window_key =
      arc::mojom::AccessibilityWindowKey::New();
  if (const absl::optional<int32_t> window_id_opt =
          exo::GetShellClientAccessibilityId(tree_source->window())) {
    window_key->set_window_id(window_id_opt.value());
  } else if (const absl::optional<int32_t> task_id =
                 arc::GetWindowTaskId(tree_source->window())) {
    window_key->set_task_id(task_id.value());
  } else {
    return false;
  }

  return accessibility_helper_instance_.RequestSendAccessibilityTree(
      std::move(window_key));
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::OnAccessibilityEvent(
    const mojom::AccessibilityEventData* const event_data) {
  DCHECK(event_data);
  bool is_notification_event = event_data->notification_key.has_value();
  if (is_notification_event) {
    const std::string& notification_key = event_data->notification_key.value();

    // This bridge must receive OnNotificationStateChanged call for the
    // notification_key before this receives an accessibility event for it.
    return GetFromKey(KeyForNotification(notification_key));
  } else if (event_data->is_input_method_window) {
    exo::InputMethodSurface* input_method_surface =
        exo::InputMethodSurface::GetInputMethodSurface();
    if (!input_method_surface)
      return nullptr;

    auto key = KeyForInputMethod();
    if (GetFromKey(key) == nullptr) {
      auto* tree = CreateFromKey(key, input_method_surface->host_window());
      input_method_surface->SetChildAxTreeId(tree->ax_tree_id());
    }

    return GetFromKey(key);
  } else {
    int task_id;
    if (event_data->task_id != kNoTaskId) {
      task_id = event_data->task_id;
    } else {
      const auto itr = window_id_to_task_id_.find(event_data->window_id);
      if (itr == window_id_to_task_id_.end())
        return nullptr;
      task_id = itr->second;
    }

    const auto window_itr = task_id_to_window_.find(task_id);
    if (window_itr == task_id_to_window_.end())
      return nullptr;

    aura::Window* window = window_itr->second;

    const auto key = KeyForTaskId(task_id);
    AXTreeSourceArc* tree_source = GetFromKey(key);
    if (!tree_source) {
      tree_source = CreateFromKey(key, window);
      SetChildAxTreeIDForWindow(window, tree_source->ax_tree_id());
    }

    UpdateWindowProperties(window);
    return tree_source;
  }
}

void ArcAccessibilityTreeTracker::OnNotificationSurfaceAdded(
    ash::ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromKey(KeyForNotification(notification_key));
  if (!tree)
    return;

  tree->set_window(surface->GetWindow());
  surface->SetAXTreeId(tree->ax_tree_id());

  // Dispatch ax::mojom::Event::kChildrenChanged to force AXNodeData of the
  // notification updated. As order of OnNotificationSurfaceAdded call is not
  // guaranteed, we are dispatching the event in both
  // ArcAccessibilityHelperBridge and ArcNotificationContentView. The event
  // needs to be dispatched after:
  // 1. ax_tree_id is set to the surface
  // 2. the surface is attached to the content view
  if (surface->IsAttached()) {
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, false);
  }
}

void ArcAccessibilityTreeTracker::OnNotificationSurfaceRemoved(
    ash::ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromKey(KeyForNotification(notification_key));
  if (!tree)
    return;

  tree->set_window(nullptr);
}

void ArcAccessibilityTreeTracker::OnNotificationStateChanged(
    const std::string& notification_key,
    const arc::mojom::AccessibilityNotificationStateType& state) {
  auto key = KeyForNotification(notification_key);
  switch (state) {
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED: {
      AXTreeSourceArc* tree_source = GetFromKey(key);
      if (tree_source)
        return;

      auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
      if (!surface_manager)
        return;

      ash::ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(notification_key);

      tree_source = CreateFromKey(std::move(key),
                                  surface ? surface->GetWindow() : nullptr);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        tree_source->ax_tree_id());
      break;
    }
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED:
      trees_.erase(key);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        ui::AXTreeIDUnknown());
      break;
  }
}

void ArcAccessibilityTreeTracker::OnAndroidVirtualKeyboardVisibilityChanged(
    bool visible) {
  if (!visible)
    trees_.erase(KeyForInputMethod());
}

void ArcAccessibilityTreeTracker::OnToggleNativeChromeVoxArcSupport(
    bool enabled) {
  // This is dispatched from Android when ArcAccessibilityHelperService changes
  // the active screen reader on Android.
  native_chromevox_enabled_ = enabled;
  DispatchCustomSpokenFeedbackToggled(!enabled);

  // TODO(hirokisato): Don't we need to do something similar in
  // OnSetNativeChromeVoxArcSupportProcessed()?
}

void ArcAccessibilityTreeTracker::SetNativeChromeVoxArcSupport(
    bool enabled,
    SetNativeChromeVoxCallback callback) {
  aura::Window* window = GetFocusedArcWindow();
  if (!window) {
    std::move(callback).Run(
        extensions::api::accessibility_private::SetNativeChromeVoxResponse::
            SET_NATIVE_CHROME_VOX_RESPONSE_FAILURE);
    return;
  }

  if (!arc::GetWindowTaskId(window).has_value()) {
    std::move(callback).Run(
        extensions::api::accessibility_private::SetNativeChromeVoxResponse::
            SET_NATIVE_CHROME_VOX_RESPONSE_FAILURE);
    return;
  }

  std::unique_ptr<aura::WindowTracker> window_tracker =
      std::make_unique<aura::WindowTracker>();
  window_tracker->Add(window);

  accessibility_helper_instance_.SetNativeChromeVoxArcSupportForFocusedWindow(
      enabled,
      base::BindOnce(
          &ArcAccessibilityTreeTracker::OnSetNativeChromeVoxArcSupportProcessed,
          base::Unretained(this), std::move(window_tracker), enabled,
          std::move(callback)));
}

void ArcAccessibilityTreeTracker::OnSetNativeChromeVoxArcSupportProcessed(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    bool enabled,
    SetNativeChromeVoxCallback callback,
    arc::mojom::SetNativeChromeVoxResponse response) {
  std::move(callback).Run(FromMojomResponseToAutomationResponse(response));

  if (response != arc::mojom::SetNativeChromeVoxResponse::SUCCESS ||
      window_tracker->windows().size() != 1) {
    return;
  }

  aura::Window* window = window_tracker->Pop();
  auto task_id = arc::GetWindowTaskId(window);
  DCHECK(task_id);

  if (enabled) {
    talkback_enabled_task_ids_.erase(*task_id);
  } else {
    trees_.erase(KeyForTaskId(*task_id));
    talkback_enabled_task_ids_.insert(*task_id);
  }

  UpdateWindowProperties(window);
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::GetFromTreeId(
    const ui::AXTreeID& tree_id) const {
  for (auto it = trees_.begin(); it != trees_.end(); ++it) {
    if (it->second->ax_tree_id() == tree_id)
      return it->second.get();
  }
  return nullptr;
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::GetFromKey(const TreeKey& key) {
  auto tree_it = trees_.find(key);
  if (tree_it == trees_.end())
    return nullptr;

  return tree_it->second.get();
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::CreateFromKey(
    TreeKey key,
    aura::Window* window) {
  auto tree = std::make_unique<AXTreeSourceArc>(tree_source_delegate_, window);
  AXTreeSourceArc* tree_ptr = tree.get();
  trees_.insert(std::make_pair(std::move(key), std::move(tree)));

  if (ash::AccessibilityManager::Get() &&
      ash::AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    // Record metrics only when SpokenFeedback is enabled in order to
    // compare this with TalkBack usage.
    base::UmaHistogramBoolean("Arc.AccessibilityWithTalkBack", false);
  }
  return tree_ptr;
}

void ArcAccessibilityTreeTracker::InvalidateTrees() {
  for (auto it = trees_.begin(); it != trees_.end(); ++it)
    it->second->InvalidateTree();
}

void ArcAccessibilityTreeTracker::TrackWindow(aura::Window* window) {
  windows_observer_->Observe(window);
  UpdateWindowIdMapping(window);
  UpdateWindowProperties(window);
}

void ArcAccessibilityTreeTracker::UpdateWindowIdMapping(aura::Window* window) {
  auto task_id = arc::GetWindowTaskId(window);
  if (!task_id.has_value())
    return;

  if (task_id_to_window_.count(task_id.value()) == 0)
    task_id_to_window_.emplace(task_id.value(), window);

  const auto window_id = exo::GetShellClientAccessibilityId(window);
  if (!window_id.has_value())
    return;

  if (window_id_to_task_id_.find(window_id.value()) !=
      window_id_to_task_id_.end()) {
    // We already know this window ID.
    return;
  }

  window_id_to_task_id_[window_id.value()] = *task_id;

  // The window ID is new to us. Request the entire tree.
  arc::mojom::AccessibilityWindowKeyPtr window_key =
      arc::mojom::AccessibilityWindowKey::New();
  window_key->set_window_id(window_id.value());
  accessibility_helper_instance_.RequestSendAccessibilityTree(
      std::move(window_key));
}

void ArcAccessibilityTreeTracker::UpdateWindowProperties(aura::Window* window) {
  if (!ash::IsArcWindow(window))
    return;

  auto task_id = arc::GetWindowTaskId(window);
  if (!task_id.has_value())
    return;

  bool use_talkback = talkback_enabled_task_ids_.count(*task_id) > 0;

  window->SetProperty(aura::client::kAccessibilityTouchExplorationPassThrough,
                      use_talkback);
  window->SetProperty(ash::kSearchKeyAcceleratorReservedKey, use_talkback);

  if (use_talkback) {
    SetChildAxTreeIDForWindow(window, ui::AXTreeIDUnknown());
  } else if (filter_type_ == arc::mojom::AccessibilityFilterType::ALL) {
    auto key = KeyForTaskId(*task_id);
    AXTreeSourceArc* tree = GetFromKey(key);
    if (!tree)
      tree = CreateFromKey(std::move(key), window);

    // Just after the creation of window, widget has not been set yet and this
    // is not dispatched to ShellSurfaceBase. Thus, call this every time.
    SetChildAxTreeIDForWindow(window, tree->ax_tree_id());
  }
}

void ArcAccessibilityTreeTracker::StartTrackingWindows() {
  DCHECK(aura::Env::HasInstance());
  env_observation_.Observe(aura::Env::GetInstance());

  for (auto* host : aura::Env::GetInstance()->window_tree_hosts())
    StartTrackingWindows(host->window());
}

void ArcAccessibilityTreeTracker::StartTrackingWindows(aura::Window* window) {
  if (ShouldTrackWindow(window)) {
    TrackWindow(window);
    return;
  }
  for (aura::Window* child : window->children())
    StartTrackingWindows(child);
}

void ArcAccessibilityTreeTracker::DispatchCustomSpokenFeedbackToggled(
    bool enabled) {
  auto event_args(extensions::api::accessibility_private::
                      OnCustomSpokenFeedbackToggled::Create(enabled));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::
          ACCESSIBILITY_PRIVATE_ON_CUSTOM_SPOKEN_FEEDBACK_TOGGLED,
      extensions::api::accessibility_private::OnCustomSpokenFeedbackToggled::
          kEventName,
      std::move(event_args)));
  extensions::EventRouter::Get(profile_)->BroadcastEvent(std::move(event));

  if (!enabled) {
    // Only record when TalkBack is enabled because ChromeVox usage is trakced
    // when AXTreeSourceArc instance is created on CreateFromKey().
    base::UmaHistogramBoolean("Arc.AccessibilityWithTalkBack", !enabled);
  }
}

aura::Window* ArcAccessibilityTreeTracker::GetFocusedArcWindow() const {
  if (!exo::WMHelper::HasInstance())
    return nullptr;
  return FindArcOrGhostWindow(exo::WMHelper::GetInstance()->GetFocusedWindow());
}

}  // namespace arc
