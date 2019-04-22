// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chromecast/browser/extensions/api/automation_internal/automation_event_router_interface.h"
#include "chromecast/common/extensions_api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_tree_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {
namespace cast {
struct AutomationListener;

class AutomationEventRouter : public ui::AXEventBundleSink,
                              public AutomationEventRouterInterface,
                              public content::NotificationObserver {
 public:
  static AutomationEventRouter* GetInstance();

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from the accessibility tree indicated by
  // |source_ax_tree_id|. Automation events are forwarded from now on until the
  // listener process dies.
  void RegisterListenerForOneTree(const ExtensionId& extension_id,
                                  int listener_process_id,
                                  ui::AXTreeID source_ax_tree_id);

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from all accessibility trees because it has Desktop
  // permission.
  void RegisterListenerWithDesktopPermission(const ExtensionId& extension_id,
                                             int listener_process_id);

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override;

  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override;

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null,
  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override;

  // Notify the source extension of the action of an action result.
  void DispatchActionResult(const ui::AXActionData& data,
                            bool result,
                            content::BrowserContext* active_profile) override;

 private:
  struct AutomationListener {
    AutomationListener();
    AutomationListener(const AutomationListener& other);
    ~AutomationListener();

    ExtensionId extension_id;
    int process_id;
    bool desktop;
    std::set<ui::AXTreeID> tree_ids;
  };

  AutomationEventRouter();
  ~AutomationEventRouter() override;

  void Register(const ExtensionId& extension_id,
                int listener_process_id,
                ui::AXTreeID source_ax_tree_id,
                bool desktop);

  // ui::AXEventBundleSink:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;

  // content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;
  std::vector<AutomationListener> listeners_;

  friend struct base::DefaultSingletonTraits<AutomationEventRouter>;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventRouter);
};

}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
