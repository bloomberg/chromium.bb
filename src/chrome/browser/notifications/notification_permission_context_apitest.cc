// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionRegistryObserver;

namespace {

// Observes loaded extensions for a given profile and automatically updates
// their permission status with the NotifierStateTracker.
class ExtensionPermissionUpdater : public ExtensionRegistryObserver {
 public:
  ExtensionPermissionUpdater(Profile* profile, bool enabled)
      : profile_(profile), enabled_(enabled) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
  }

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    DCHECK(extension);

    NotifierStateTracker* notifier_state_tracker =
        NotifierStateTrackerFactory::GetForProfile(profile_);
    DCHECK(notifier_state_tracker);

    notifier_state_tracker->SetNotifierEnabled(
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   extension->id()),
        enabled_);
  }

 private:
  Profile* profile_;
  bool enabled_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionPermissionUpdater);
};

using NotificationPermissionContextApiTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(NotificationPermissionContextApiTest, Granted) {
  // Verifies that all JavaScript mechanisms exposed to extensions consistently
  // report permission status when permission has been granted.
  ExtensionPermissionUpdater updater(profile(), /* enabled= */ true);

  ASSERT_TRUE(
      RunExtensionTestWithArg("notifications/permissions_test", "granted"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationPermissionContextApiTest, Denied) {
  // Verifies that all JavaScript mechanisms exposed to extensions consistently
  // report permission status when permission has been denied.
  ExtensionPermissionUpdater updater(profile(), /* enabled= */ false);

  ASSERT_TRUE(
      RunExtensionTestWithArg("notifications/permissions_test", "denied"))
      << message_;
}

}  // namespace
