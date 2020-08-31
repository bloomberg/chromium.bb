// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/resource/scale_factor.h"

class Profile;
class PrefService;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace vm_tools {
namespace apps {
class ApplicationList;
}  // namespace apps
}  // namespace vm_tools

namespace guest_os {

// The GuestOsRegistryService  stores information about Desktop Entries (apps)
// in Crostini. We store this in prefs so that it is readily available even when
// the VM isn't running. The registrations here correspond to .desktop files,
// which are detailed in the spec:
// https://www.freedesktop.org/wiki/Specifications/desktop-entry-spec/

// This class deals with several types of IDs, including:
// 1) Desktop File IDs (desktop_file_id):
//    - As per the desktop entry spec.
// 2) Crostini App List Ids (app_id):
//    - Valid extensions ids for apps stored in the registry, derived from the
//    desktop file id, vm name, and container name.
//    - The Terminal is a special case, using kCrostiniTerminalId (see below).
// 3) Exo Window App Ids (window_app_id):
//    - Retrieved from exo::GetShellApplicationId()
//    - For Wayland apps, this is the surface class of the app
//    - For X apps, this is of the form org.chromium.termina.wmclass.foo when
//    WM_CLASS is set to foo, or otherwise some string prefixed by
//    "org.chromium.termina." when WM_CLASS is not set.
// 4) Shelf App Ids (shelf_app_id):
//    - Used in ash::ShelfID::app_id
//    - Either a Window App Id prefixed by "crostini:" or a Crostini App Id.
//    - For pinned apps, this is a Crostini App Id.

// The default Terminal app does not correspond to a desktop file, but users
// of the registry can treat it as a regular app that is always installed.
// Internal to the registry, the pref entry only contains the last launch time
// so some care is required.
class GuestOsRegistryService : public KeyedService {
 public:
  using VmType = vm_tools::apps::ApplicationList_VmType;

  class Registration {
   public:
    Registration(const base::Value* pref, bool is_terminal_app);
    Registration(Registration&& registration) = default;
    Registration& operator=(Registration&& registration) = default;
    ~Registration();

    std::string DesktopFileId() const;
    VmType VmType() const;
    std::string VmName() const;
    std::string ContainerName() const;

    std::string Name() const;
    std::string Comment() const;
    std::string ExecutableFileName() const;
    std::set<std::string> Extensions() const;
    std::set<std::string> MimeTypes() const;
    std::set<std::string> Keywords() const;
    bool NoDisplay() const;

    std::string PackageId() const;

    base::Time InstallTime() const;
    base::Time LastLaunchTime() const;

    // Whether this app should scale up when displayed.
    bool IsScaled() const;
    bool CanUninstall() const;

    // Whether this app is the default terminal app.
    bool is_terminal_app() const { return is_terminal_app_; }

   private:
    std::string LocalizedString(base::StringPiece key) const;
    std::set<std::string> LocalizedList(base::StringPiece key) const;

    // The pref can only be null when the registration is for the Terminal app.
    // If we do have a pref for the Terminal app, it contains only the last
    // launch time.
    base::Value pref_;
    bool is_terminal_app_;

    DISALLOW_COPY_AND_ASSIGN(Registration);
  };

  class Observer {
   public:
    // Called at the end of UpdateApplicationList() with lists of app_ids for
    // apps which have been updated, removed, and inserted. Not called when the
    // last_launch_time field is updated.
    virtual void OnRegistryUpdated(
        guest_os::GuestOsRegistryService* registry_service,
        const std::vector<std::string>& updated_apps,
        const std::vector<std::string>& removed_apps,
        const std::vector<std::string>& inserted_apps) {}

    // Called when an icon has been installed for the specified app so loading
    // of that icon should be requested again.
    virtual void OnAppIconUpdated(const std::string& app_id,
                                  ui::ScaleFactor scale_factor) {}

   protected:
    virtual ~Observer() = default;
  };

  explicit GuestOsRegistryService(Profile* profile);
  ~GuestOsRegistryService() override;

  base::WeakPtr<GuestOsRegistryService> GetWeakPtr();

  // Return all installed apps. This always includes the Terminal app.
  std::map<std::string, GuestOsRegistryService::Registration>
  GetAllRegisteredApps() const;

  // Return all installed apps for a given vm.
  // If |vm_type == TERMINA_VM| then this includes the Terminal app.
  std::map<std::string, GuestOsRegistryService::Registration> GetRegisteredApps(
      VmType vm_type) const;

  // Return null if |app_id| is not found in the registry.
  base::Optional<GuestOsRegistryService::Registration> GetRegistration(
      const std::string& app_id) const;

  // Constructs path to app icon for specific scale factor.
  base::FilePath GetIconPath(const std::string& app_id,
                             ui::ScaleFactor scale_factor) const;

  // Calls RequestIcon if no request is recorded.
  void MaybeRequestIcon(const std::string& app_id,
                        ui::ScaleFactor scale_factor);

  // Remove all apps from the named VM and container. If |container_name| is an
  // empty string, this function removes all apps associated with the VM,
  // regardless of container. Used in the uninstall process.
  void ClearApplicationList(const std::string& vm_name,
                            const std::string& container_name);

  // Remove all apps from the named container. Used when deleting a container
  // without deleting the whole VM.
  void ClearApplicationListForContainer(const std::string& vm_name,
                                        const std::string& container_name);

  // The existing list of apps is replaced by |application_list|.
  void UpdateApplicationList(const vm_tools::apps::ApplicationList& app_list);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notify the registry to update the last_launched field.
  void AppLaunched(const std::string& app_id);

  // Serializes the current time and stores it in |dictionary|.
  void SetCurrentTime(base::Value* dictionary, const char* key) const;

  // Set the display scaled setting of the |app_id| to |scaled|.
  void SetAppScaled(const std::string& app_id, bool scaled);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

 private:
  // Run start up tasks for the registry (e.g. recording metrics).
  void RecordStartupMetrics();

  // Construct path to app local data.
  base::FilePath GetAppPath(const std::string& app_id) const;
  // Called to request an icon from the container.
  void RequestIcon(const std::string& app_id, ui::ScaleFactor scale_factor);
  // Callback for when we request an icon from the container.
  void OnContainerAppIcon(const std::string& app_id,
                          ui::ScaleFactor scale_factor,
                          bool success,
                          const std::vector<crostini::Icon>& icons);
  // Callback for our internal call for saving out icon data.
  void OnIconInstalled(const std::string& app_id,
                       ui::ScaleFactor scale_factor,
                       bool success);
  // Removes all the icons installed for an application.
  void RemoveAppData(const std::string& app_id);

  // Migrates terminal from old crosh-based terminal to new Terminal System App.
  // Old terminal is removed from registry, and launcher position and pinned
  // attribute is copied to the new terminal.
  // TODO(crbug.com/1019021):  Keep this code for at least 1 release after
  // TerminalSystemApp feature is removed.  Current expectation is to remove
  // feature in M83, this function can then be remoevd after M84.
  void MigrateTerminal() const;

  // Owned by the Profile.
  Profile* const profile_;
  PrefService* const prefs_;

  // Keeps root folder where Crostini app icons for different scale factors are
  // stored.
  base::FilePath base_icon_path_;

  base::ObserverList<Observer>::Unchecked observers_;

  const base::Clock* clock_;

  // Keeps record for icon request to avoid duplication. Each app may contain
  // several requests for different scale factor. Scale factor is defined by
  // specific bit position. The |active_icon_requests_| holds icon request that
  // are either in flight or have been completed successfully so they should not
  // be requested again. |retry_icon_requests| holds failed requests which we
  // should attempt again when we get an app list refresh from the container
  // which means there's a good chance the container is online and the request
  // will then succeed.
  std::map<std::string, uint32_t> active_icon_requests_;
  std::map<std::string, uint32_t> retry_icon_requests_;

  base::WeakPtrFactory<GuestOsRegistryService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GuestOsRegistryService);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_
