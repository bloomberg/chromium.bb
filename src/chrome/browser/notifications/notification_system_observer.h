// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

class NotificationUIManager;

// The content::NotificationObserver observes system status change and sends
// events to NotificationUIManager. NOTE: NotificationUIManager is deprecated,
// to be replaced by NotificationDisplayService, so this class should go away.
class NotificationSystemObserver : public content::NotificationObserver,
                                   extensions::ExtensionRegistryObserver {
 public:
  explicit NotificationSystemObserver(NotificationUIManager* ui_manager);
  NotificationSystemObserver(const NotificationSystemObserver&) = delete;
  NotificationSystemObserver& operator=(const NotificationSystemObserver&) =
      delete;
  ~NotificationSystemObserver() override;

 protected:
  // content::NotificationObserver override.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::ExtensionRegistryObserver override.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

 private:
  // Registrar for the other kind of notifications (event signaling).
  content::NotificationRegistrar registrar_;
  raw_ptr<NotificationUIManager> ui_manager_;

  base::ScopedMultiSourceObservation<extensions::ExtensionRegistry,
                                     extensions::ExtensionRegistryObserver>
      extension_registry_observations_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
