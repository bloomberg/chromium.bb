// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_remote_host.h"

#include <stddef.h>

#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/mus/ax_tree_source_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using display::Display;
using display::Screen;

namespace views {

// For external linkage.
constexpr int AXRemoteHost::kRemoteAXTreeID;

AXRemoteHost::AXRemoteHost() {
  AXAuraObjCache::GetInstance()->SetDelegate(this);
}

AXRemoteHost::~AXRemoteHost() {
  if (widget_)
    StopMonitoringWidget();
  AXAuraObjCache::GetInstance()->SetDelegate(nullptr);
}

void AXRemoteHost::Init(service_manager::Connector* connector) {
  connector->BindInterface(ax::mojom::kAXHostServiceName, &ax_host_ptr_);
  BindAndSetRemote();
}

void AXRemoteHost::InitForTesting(ax::mojom::AXHostPtr host_ptr) {
  ax_host_ptr_ = std::move(host_ptr);
  BindAndSetRemote();
}

void AXRemoteHost::StartMonitoringWidget(Widget* widget) {
  if (!enabled_)
    return;

  // Check if we're already tracking a widget.
  // TODO(jamescook): Support multiple widgets.
  if (widget_)
    return;
  widget_ = widget;
  widget_->AddObserver(this);

  // The cache needs to track the root window to follow focus changes.
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  cache->OnRootWindowObjCreated(widget_->GetNativeWindow());

  // Start the AX tree with the contents view because the window frame is
  // handled by the window manager in another process.
  View* contents_view = widget_->widget_delegate()->GetContentsView();
  AXAuraObjWrapper* contents_wrapper = cache->GetOrCreate(contents_view);

  tree_source_ = std::make_unique<AXTreeSourceMus>(contents_wrapper);
  tree_serializer_ = std::make_unique<AuraAXTreeSerializer>(tree_source_.get());

  // Inform the serializer of the display device scale factor.
  UpdateDeviceScaleFactor();
  Screen::GetScreen()->AddObserver(this);

  SendEvent(contents_wrapper, ax::mojom::Event::kLoadComplete);
}

void AXRemoteHost::StopMonitoringWidget() {
  DCHECK(widget_);
  DCHECK(widget_->HasObserver(this));
  Screen::GetScreen()->RemoveObserver(this);
  widget_->RemoveObserver(this);
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  cache->OnRootWindowObjDestroyed(widget_->GetNativeWindow());
  cache->Remove(widget_->widget_delegate()->GetContentsView());
  widget_ = nullptr;
  // Delete source and serializers to save memory.
  tree_serializer_.reset();
  tree_source_.reset();
}

void AXRemoteHost::HandleEvent(View* view, ax::mojom::Event event_type) {
  if (!enabled_)
    return;

  AXAuraObjWrapper* aura_obj =
      view ? AXAuraObjCache::GetInstance()->GetOrCreate(view)
           : tree_source_->GetRoot();
  SendEvent(aura_obj, event_type);
}

void AXRemoteHost::OnAutomationEnabled(bool enabled) {
  if (enabled)
    Enable();
  else
    Disable();
}

void AXRemoteHost::PerformAction(const ui::AXActionData& action_data) {
  if (!enabled_)
    return;

  // A hit test requires determining the node to perform the action on first.
  if (action_data.action == ax::mojom::Action::kHitTest) {
    PerformHitTest(action_data);
    return;
  }

  tree_source_->HandleAccessibleAction(action_data);
}

void AXRemoteHost::OnWidgetClosing(Widget* widget) {
  // In the typical case, clean up early during widget close. Waiting until
  // widget destroy can sometimes lead to crashes due to AX window visibility
  // events being triggered during close. https://crbug.com/869608
  DCHECK_EQ(widget_, widget);
  StopMonitoringWidget();
}

void AXRemoteHost::OnWidgetDestroying(Widget* widget) {
  // Widgets can be deleted without being closed, which is common in tests
  // and possible in production.
  DCHECK_EQ(widget_, widget);
  StopMonitoringWidget();
}

void AXRemoteHost::OnDisplayMetricsChanged(const Display& display,
                                           uint32_t metrics) {
  if (metrics & display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR) {
    UpdateDeviceScaleFactor();
    SendEvent(tree_source_->GetRoot(), ax::mojom::Event::kLocationChanged);
  }
}

void AXRemoteHost::OnChildWindowRemoved(AXAuraObjWrapper* parent) {
  if (!enabled_ || !widget_)
    return;

  DCHECK(tree_source_);
  if (!parent)
    parent = tree_source_->GetRoot();

  SendEvent(parent, ax::mojom::Event::kChildrenChanged);
}

void AXRemoteHost::OnEvent(AXAuraObjWrapper* aura_obj,
                           ax::mojom::Event event_type) {
  SendEvent(aura_obj, event_type);
}

void AXRemoteHost::FlushForTesting() {
  ax_host_ptr_.FlushForTesting();
}

void AXRemoteHost::BindAndSetRemote() {
  ax::mojom::AXRemoteHostPtr remote;
  binding_.Bind(mojo::MakeRequest(&remote));
  ax_host_ptr_->SetRemoteHost(std::move(remote));
}

void AXRemoteHost::Enable() {
  // Extensions can send multiple enable events.
  if (enabled_)
    return;
  enabled_ = true;

  std::set<aura::Window*> roots =
      MusClient::Get()->window_tree_client()->GetRoots();
  for (aura::Window* root : roots) {
    Widget* widget = Widget::GetWidgetForNativeWindow(root);
    // Typically it's only tests that create Windows (the WindowTreeHost
    // backing the root Window) in such a way that there is no associated
    // widget.
    if (widget) {
      // TODO(jamescook): Support multiple roots.
      DCHECK(!widget_);
      StartMonitoringWidget(widget);
    }
  }
}

void AXRemoteHost::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  StopMonitoringWidget();
}

void AXRemoteHost::SendEvent(AXAuraObjWrapper* aura_obj,
                             ax::mojom::Event event_type) {
  DCHECK(aura_obj);
  if (!enabled_ || !widget_)
    return;

  DCHECK(tree_source_);
  DCHECK(tree_serializer_);

  ui::AXTreeUpdate update;
  if (!tree_serializer_->SerializeChanges(aura_obj, &update)) {
    LOG(ERROR) << "Unable to serialize accessibility tree.";
    return;
  }

  std::vector<ui::AXTreeUpdate> updates;
  updates.push_back(update);

  // Make sure the focused node is serialized.
  AXAuraObjWrapper* focus = AXAuraObjCache::GetInstance()->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    tree_serializer_->SerializeChanges(focus, &focused_node_update);
    updates.push_back(focused_node_update);
  }

  ui::AXEvent event;
  event.id = aura_obj->GetUniqueId().Get();
  event.event_type = event_type;
  // Other fields are not used.

  ax_host_ptr_->HandleAccessibilityEvent(kRemoteAXTreeID, updates, event);
}

void AXRemoteHost::PerformHitTest(const ui::AXActionData& action) {
  DCHECK(enabled_);
  DCHECK_EQ(action.action, ax::mojom::Action::kHitTest);

  // If the widget isn't open yet there's nothing to hit.
  if (!widget_)
    return;

  views::View* hit_view =
      widget_->GetRootView()->GetEventHandlerForPoint(action.target_point);
  if (hit_view)
    hit_view->NotifyAccessibilityEvent(action.hit_test_event_to_fire, true);
}

void AXRemoteHost::UpdateDeviceScaleFactor() {
  DCHECK(widget_);
  DCHECK(tree_source_);

  // Use the scale factor for the widget's window's current display.
  Display display =
      Screen::GetScreen()->GetDisplayNearestWindow(widget_->GetNativeWindow());
  tree_source_->set_device_scale_factor(display.device_scale_factor());
}

}  // namespace views
