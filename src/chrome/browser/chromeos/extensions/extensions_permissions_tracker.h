// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSIONS_PERMISSIONS_TRACKER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSIONS_PERMISSIONS_TRACKER_H_

#include "base/scoped_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Used to track the installation of the force-installed extensions of the
// managed-guest session to decide whether the permissions of the extensions
// should trigger the full warning on the login screen or not. The result is
// saved in the local state perf, and the login screen warning of the managed
// guest session is updated accordingly. ExtensionSystemImpl owns this class and
// outlives it.
class ExtensionsPermissionsTracker : public ExtensionRegistryObserver {
 public:
  ExtensionsPermissionsTracker(ExtensionRegistry* registry,
                               content::BrowserContext* browser_context);
  ExtensionsPermissionsTracker(const ExtensionsPermissionsTracker&) = delete;
  ExtensionsPermissionsTracker& operator=(const ExtensionsPermissionsTracker&) =
      delete;
  ~ExtensionsPermissionsTracker() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

 protected:
  virtual bool IsSafePerms(const PermissionsData* perms_data) const;

 private:
  // Loads list of force-installed extensions if it's available.
  void OnForcedExtensionsPrefChanged();

  void UpdateLocalState();
  void ParseExtensionPermissions(const Extension* extension);

  // Unowned, but guaranteed to outlive this object.
  ExtensionRegistry* registry_;

  // Unowned, but guaranteed to outlive this object.
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  // // Whether the extension is considered "safe". If any extension is not, it
  // will result in the full warning being shown on the login screen.
  std::map<ExtensionId, bool> extension_safety_ratings_;

  // Set of not yet loaded force installed extensions.
  std::set<ExtensionId> pending_forced_extensions_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSIONS_PERMISSIONS_TRACKER_H_
