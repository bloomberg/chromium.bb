// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOGGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log_collector.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace enterprise_management {
class ExtensionInstallReportLogEvent;
}

namespace policy {

// TODO(crbug/1073436): Refactor out common methods of this class and
// AppInstallEventLogger to new class.

// Ensure that events related to extension installation process are logged. It
// observes the kInstallForceList pref and keeps track of the forced extensions
// to be installed. Additionally, an |ExtensionInstallEventLogCollector| is
// instantiated to collect detailed logs of the extension installation process
// whenever there is at least one pending installation request.
class ExtensionInstallEventLogger
    : public ExtensionInstallEventLogCollector::Delegate {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every extension in |extensions|.
    virtual void Add(
        const std::set<extensions::ExtensionId>& extensions,
        const enterprise_management::ExtensionInstallReportLogEvent& event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  ExtensionInstallEventLogger(Delegate* delegate,
                              Profile* profile,
                              extensions::ExtensionRegistry* registry);
  ~ExtensionInstallEventLogger() override;

  // Clears all data related to extension-install event log collection for
  // |profile|. Must not be called while an |ExtensionInstallEventLogger| exists
  // for |profile|.
  static void Clear(Profile* profile);

  // ExtensionInstallEventLogCollector::Delegate:
  void AddForAllExtensions(
      std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
          event) override;
  void Add(
      const extensions::ExtensionId& extension_id,
      bool gather_disk_space_info,
      std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
          event) override;
  void OnExtensionInstallationFinished(
      const extensions::ExtensionId& extension_id) override;
  bool IsExtensionPending(const extensions::ExtensionId& extension_id) override;

 private:
  // Loads list of force-installed extensions if available.
  void OnForcedExtensionsPrefChanged();

  // Informs the existing |log_collector_| that the list of extension
  // install requests has changed or instantiates a new |log_collector_| if
  // none exists yet.
  void UpdateCollector();

  // Destroys the |log_collector_|, if it exists.
  void StopCollector();

  // Stores a list of pending extensions into a pref.
  void SetInstallPendingPref(
      const std::set<extensions::ExtensionId>& extensions);

  // Adds information about total and free disk space to |event|, then adds
  // |event| to the log for every extension in |extensions|.
  void AddForSetOfExtensionsWithDiskSpaceInfo(
      const std::set<extensions::ExtensionId>& extensions,
      std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
          event);

  // Adds |event| to the log for every extension in |extensions|.
  void AddForSetOfExtensions(
      const std::set<extensions::ExtensionId>& extensions,
      std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
          event);

  // The set of forced installed extensions updated from the forced list policy.
  std::set<extensions::ExtensionId> extensions_;

  // The set of forced installed extensions that are currently not loaded and
  // are not failed.
  std::set<extensions::ExtensionId> pending_extensions_;

  // The delegate that events are forwarded to for inclusion in the log.
  Delegate* const delegate_;

  // The profile whose extension install requests to log.
  Profile* const profile_;

  extensions::ExtensionRegistry* registry_;

  // Unowned, but guaranteed to outlive this object.
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are calling OnForcedExtensionPrefChanged for the first time or
  // not. In the initial case we would have to add the login event for the
  // extensions.
  bool initial_ = true;

  // The |ExtensionInstallEventLogCollector| that collects detailed logs of the
  // extension install process. Non-|nullptr| whenever there are one or more
  // pending extension install requests.
  std::unique_ptr<ExtensionInstallEventLogCollector> log_collector_;

  // Weak factory used to reference |this| from background tasks.
  base::WeakPtrFactory<ExtensionInstallEventLogger> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOGGER_H_
