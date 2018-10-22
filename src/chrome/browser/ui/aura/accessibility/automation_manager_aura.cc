// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/chromeos/accessibility/ax_host_service.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget_delegate.h"
#endif

using content::BrowserContext;
using extensions::AutomationEventRouter;

namespace {

// Returns default browser context for sending events in case it was not
// provided. This works around a crash in profile creation during OOBE when
// accessibility is enabled. https://crbug.com/738003
BrowserContext* GetDefaultEventContext() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

#if defined(OS_CHROMEOS)
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // It is not guaranteed that user profile creation is completed for
  // some session states. In this case use default profile.
  const session_manager::SessionState session_state =
      session_manager ? session_manager->session_state()
                      : session_manager::SessionState::UNKNOWN;
  switch (session_state) {
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
    case session_manager::SessionState::LOCKED:
      break;
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
      const base::FilePath defult_profile_dir =
          profiles::GetDefaultProfileDir(profile_manager->user_data_dir());
      return profile_manager->GetProfileByPath(defult_profile_dir);
  }
#endif

  return ProfileManager::GetLastUsedProfile();
}

}  // namespace

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  return base::Singleton<AutomationManagerAura>::get();
}

void AutomationManagerAura::Enable(BrowserContext* context) {
  enabled_ = true;
  Reset(false);

  SendEvent(context, current_tree_->GetRoot(), ax::mojom::Event::kLoadComplete);
  views::AXAuraObjCache::GetInstance()->SetDelegate(this);

#if defined(OS_CHROMEOS)
  // TODO(crbug.com/756054): Support SingleProcessMash and MultiProcessMash.
  if (!features::IsUsingWindowService()) {
    aura::Window* active_window = ash::wm::GetActiveWindow();
    if (active_window) {
      views::AXAuraObjWrapper* focus =
          views::AXAuraObjCache::GetInstance()->GetOrCreate(active_window);
      SendEvent(context, focus, ax::mojom::Event::kChildrenChanged);
    }
  }
  // Gain access to out-of-process native windows.
  AXHostService::SetAutomationEnabled(true);
#endif
}

void AutomationManagerAura::Disable() {
  enabled_ = false;
  Reset(true);

#if defined(OS_CHROMEOS)
  AXHostService::SetAutomationEnabled(false);
#endif
}

void AutomationManagerAura::HandleEvent(BrowserContext* context,
                                        views::View* view,
                                        ax::mojom::Event event_type) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* aura_obj = view ?
      views::AXAuraObjCache::GetInstance()->GetOrCreate(view) :
      current_tree_->GetRoot();
  SendEvent(nullptr, aura_obj, event_type);
}

void AutomationManagerAura::HandleAlert(content::BrowserContext* context,
                                        const std::string& text) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* obj =
      static_cast<AXRootObjWrapper*>(current_tree_->GetRoot())
          ->GetAlertForText(text);
  SendEvent(context, obj, ax::mojom::Event::kAlert);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  CHECK(enabled_);

  // Unlike all of the other actions, a hit test requires determining the
  // node to perform the action on first.
  if (data.action == ax::mojom::Action::kHitTest) {
    PerformHitTest(data);
    return;
  }

  current_tree_->HandleAccessibleAction(data);
}

void AutomationManagerAura::OnChildWindowRemoved(
    views::AXAuraObjWrapper* parent) {
  if (!enabled_)
    return;

  if (!parent)
    parent = current_tree_->GetRoot();

  SendEvent(nullptr, parent, ax::mojom::Event::kChildrenChanged);
}

void AutomationManagerAura::OnEvent(views::AXAuraObjWrapper* aura_obj,
                                    ax::mojom::Event event_type) {
  SendEvent(nullptr, aura_obj, event_type);
}

AutomationManagerAura::AutomationManagerAura()
    : AXHostDelegate(extensions::api::automation::kDesktopTreeID),
      enabled_(false),
      processing_events_(false) {}

AutomationManagerAura::~AutomationManagerAura() {
}

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_)
    current_tree_.reset(new AXTreeSourceAura());
  reset_serializer ? current_tree_serializer_.reset()
                   : current_tree_serializer_.reset(
                         new AuraAXTreeSerializer(current_tree_.get()));
}

void AutomationManagerAura::SendEvent(BrowserContext* context,
                                      views::AXAuraObjWrapper* aura_obj,
                                      ax::mojom::Event event_type) {
  if (!current_tree_serializer_)
    return;

  if (!context)
    context = GetDefaultEventContext();

  if (!context) {
    LOG(WARNING) << "Accessibility notification but no browser context";
    return;
  }

  if (processing_events_) {
    pending_events_.push_back(std::make_pair(aura_obj, event_type));
    return;
  }
  processing_events_ = true;

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = extensions::api::automation::kDesktopTreeID;
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  ui::AXTreeUpdate update;
  if (!current_tree_serializer_->SerializeChanges(aura_obj, &update)) {
    LOG(ERROR) << "Unable to serialize one accessibility event.";
    return;
  }
  event_bundle.updates.push_back(update);

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus =
      views::AXAuraObjCache::GetInstance()->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    current_tree_serializer_->SerializeChanges(focus, &focused_node_update);
    event_bundle.updates.push_back(focused_node_update);
  }

  ui::AXEvent event;
  event.id = aura_obj->GetUniqueId().Get();
  event.event_type = event_type;
  event_bundle.events.push_back(event);

  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvents(event_bundle);

  processing_events_ = false;
  auto pending_events_copy = pending_events_;
  pending_events_.clear();
  for (size_t i = 0; i < pending_events_copy.size(); ++i) {
    SendEvent(context,
              pending_events_copy[i].first,
              pending_events_copy[i].second);
  }
}

void AutomationManagerAura::PerformHitTest(
    const ui::AXActionData& original_action) {
#if defined(OS_CHROMEOS)
  ui::AXActionData action = original_action;
  aura::Window* root_window = ash::Shell::Get()->GetPrimaryRootWindow();
  if (!root_window)
    return;

  // Determine which aura Window is associated with the target point.
  aura::Window* window =
      root_window->GetEventHandlerForPoint(action.target_point);
  if (!window)
    return;

  // Convert point to local coordinates of the hit window.
  aura::Window::ConvertPointToTarget(root_window, window, &action.target_point);

  // Check for a AX node tree in a remote process (e.g. renderer, mojo app).
  ui::AXTreeIDRegistry::AXTreeID child_ax_tree_id;
  if (ash::Shell::HasRemoteClient(window)) {
    // For remote mojo apps, the |window| is a DesktopNativeWidgetAura, so the
    // parent is the widget and the widget's contents view has the child tree.
    CHECK(window->parent());
    views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(window->parent());
    CHECK(widget);
    ui::AXNodeData node_data;
    widget->widget_delegate()->GetContentsView()->GetAccessibleNodeData(
        &node_data);
    child_ax_tree_id =
        node_data.GetIntAttribute(ax::mojom::IntAttribute::kChildTreeId);
    DCHECK_NE(child_ax_tree_id, ui::AXTreeIDRegistry::kNoAXTreeID);
    DCHECK_NE(child_ax_tree_id, extensions::api::automation::kDesktopTreeID);
  } else {
    // For normal windows the (optional) child tree is an aura window property.
    child_ax_tree_id = window->GetProperty(ui::kChildAXTreeID);
  }

  // If the window has a child AX tree ID, forward the action to the
  // associated AXHostDelegate or RenderFrameHost.
  if (child_ax_tree_id != ui::AXTreeIDRegistry::kNoAXTreeID) {
    ui::AXTreeIDRegistry* registry = ui::AXTreeIDRegistry::GetInstance();
    ui::AXHostDelegate* delegate = registry->GetHostDelegate(child_ax_tree_id);
    if (delegate) {
      delegate->PerformAction(action);
      return;
    }

    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromAXTreeID(child_ax_tree_id);
    if (rfh) {
      // Convert to pixels for the RenderFrameHost HitTest.
      window->GetHost()->ConvertDIPToPixels(&action.target_point);
      rfh->AccessibilityPerformAction(action);
    }
    return;
  }

  // If the window doesn't have a child tree ID, try to fire the event
  // on a View.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    views::View* root_view = widget->GetRootView();
    views::View* hit_view =
        root_view->GetEventHandlerForPoint(action.target_point);
    if (hit_view) {
      hit_view->NotifyAccessibilityEvent(action.hit_test_event_to_fire, true);
      return;
    }
  }

  // Otherwise, fire the event directly on the Window.
  views::AXAuraObjWrapper* window_wrapper =
      views::AXAuraObjCache::GetInstance()->GetOrCreate(window);
  if (window_wrapper)
    SendEvent(nullptr, window_wrapper, action.hit_test_event_to_fire);
#endif
}
