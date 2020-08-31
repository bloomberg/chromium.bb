// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/resource/scale_factor.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

class Profile;

// TODO(crbug.com/1004708): Move Is*[Enabled|Allowed] functions to
// CrostiniFeatures.
namespace crostini {

struct LinuxPackageInfo;

// A unique identifier for our containers.
struct ContainerId {
  ContainerId(std::string vm_name, std::string container_name) noexcept;

  std::string vm_name;
  std::string container_name;
};

bool operator<(const ContainerId& lhs, const ContainerId& rhs) noexcept;
bool operator==(const ContainerId& lhs, const ContainerId& rhs) noexcept;
inline bool operator!=(const ContainerId& lhs,
                       const ContainerId& rhs) noexcept {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream,
                         const ContainerId& container_id);

using LaunchCrostiniAppCallback =
    base::OnceCallback<void(bool success, const std::string& failure_reason)>;

// Checks if user profile is able to a crostini app with a given app_id.
bool IsUninstallable(Profile* profile, const std::string& app_id);

// Returns whether the default Crostini VM is running for the user.
bool IsCrostiniRunning(Profile* profile);

// Whether the user is able to perform a container upgrade.
bool ShouldAllowContainerUpgrade(Profile* profile);

// Returns whether default Crostini container should be configured according to
// the configuration specified by CrostiniAnsiblePlaybook user policy.
bool ShouldConfigureDefaultContainer(Profile* profile);

// Returns whether a dialog from Crostini is blocking the immediate launch.
bool MaybeShowCrostiniDialogBeforeLaunch(Profile* profile,
                                         CrostiniResult result);

// Launches the Crostini app with ID of |app_id| on the display with ID of
// |display_id|. |app_id| should be a valid Crostini app list id.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id);

// Launch a Crostini App with a given set of files, given as absolute paths in
// the container. For apps which can only be launched with a single file,
// launch multiple instances.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<storage::FileSystemURL>& files,
                       LaunchCrostiniAppCallback callback);

// Convenience wrapper around CrostiniAppIconLoader. As requesting icons from
// the container can be slow, we just use the default (penguin) icons after the
// timeout elapses. Subsequent calls would get the correct icons once loaded.
void LoadIcons(Profile* profile,
               const std::vector<std::string>& app_ids,
               int resource_size_in_dip,
               ui::ScaleFactor scale_factor,
               base::TimeDelta timeout,
               base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)>
                   icons_loaded_callback);

// Retrieves cryptohome_id from profile.
std::string CryptohomeIdForProfile(Profile* profile);

// Retrieves username from profile.
std::string DefaultContainerUserNameForProfile(Profile* profile);

// Returns the mount directory within the container where paths from the Chrome
// OS host such as within Downloads are shared with the container.
base::FilePath ContainerChromeOSBaseDirectory();

// The Terminal opens Crosh but overrides the Browser's app_name so that we can
// identify it as the Crostini Terminal. In the future, we will also use these
// for Crostini apps marked Terminal=true in their .desktop file.
std::string AppNameFromCrostiniAppId(const std::string& id);

// Returns nullopt for a non-Crostini app name.
base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name);

// Returns a list of ports currently being forwarded in Crostini as a JSON
// object.
std::string GetActivePortListAsJSON(Profile* profile);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniUISurface { kSettings = 0, kAppList = 1, kCount };

// See chrome/browser/ui/views/crostini for implementation of the ShowXXX
// functions below.

// Shows the Crostini Uninstaller dialog.
void ShowCrostiniUninstallerView(Profile* profile,
                                 CrostiniUISurface ui_surface);
bool IsCrostiniRecoveryViewShowing();

// Shows the Crostini App installer dialog.
void ShowCrostiniAppInstallerView(Profile* profile,
                                  const LinuxPackageInfo& package_info);
// Shows the Crostini force-close dialog. If |app_name| is nonempty, the dialog
// will include the window's name as text. Returns a handle to that dialog, so
// that we can add observers to the dialog itself.
views::Widget* ShowCrostiniForceCloseDialog(
    const std::string& app_name,
    views::Widget* closable_widget,
    base::OnceClosure force_close_callback);
// Shows the Crostini Termina Upgrade dialog (for blocking crostini start until
// Termina version matches).
void ShowCrostiniUpdateComponentView(Profile* profile,
                                     CrostiniUISurface ui_surface);
// Shows the ui with the error message when installing a package fails.
void ShowCrostiniPackageInstallFailureView(const std::string& error_message);

// Shows the Crostini Container Upgrade dialog (for running upgrades in the
// container).
void ShowCrostiniUpdateFilesystemView(Profile* profile,
                                      CrostiniUISurface ui_surface);
// Show the Crostini Container Upgrade dialog after a delay
// (CloseCrostiniUpdateFilesystemView will cancel the next dialog show).
void PrepareShowCrostiniUpdateFilesystemView(Profile* profile,
                                             CrostiniUISurface ui_surface);
// Closes the current CrostiniUpdateFilesystemView or ensures that the view will
// not open until PrepareShowCrostiniUpdateFilesystemView is called again.
void CloseCrostiniUpdateFilesystemView();

// Show the Crostini Software Config dialog (for installing Ansible and
// applying an Ansible playbook in the container).
void ShowCrostiniAnsibleSoftwareConfigView(Profile* profile);

// Show the Crostini Recovery dialog when Crostini is still running after a
// Chrome crash. Returns false if recovery terminal can be launched.
bool ShowCrostiniRecoveryView(Profile* profile,
                              CrostiniUISurface ui_surface,
                              const std::string& app_id,
                              int64_t display_id,
                              LaunchCrostiniAppCallback callback);

// Returns App ID of the terminal app which is either the older crosh-based
// terminal, or the new Terminal System App if the TerminalSystemApp feature
// is enabled.
const std::string& GetTerminalId();

// Returns the alternative terminal ID to |GetTerminalId|.  This is used when
// migrating terminals when TerminalSystemApp feature changes.
const std::string& GetDeletedTerminalId();

// We use an arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
constexpr char kCrostiniTerminalId[] = "oajcgpnkmhaalajejhlfpacbiokdnnfe";
// web_app::GenerateAppIdFromURL(
//     GURL("chrome-untrusted://terminal/html/terminal.html"))
constexpr char kCrostiniTerminalSystemAppId[] =
    "fhicihalidkgcimdmhpohldehjmcabcf";

constexpr char kCrostiniDefaultVmName[] = "termina";
constexpr char kCrostiniDefaultContainerName[] = "penguin";
constexpr char kCrostiniDefaultUsername[] = "emperor";
// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
constexpr char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";
constexpr char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers/%d";
constexpr char kCrostiniStretchImageAlias[] = "debian/stretch";
constexpr char kCrostiniBusterImageAlias[] = "debian/buster";

constexpr base::FilePath::CharType kHomeDirectory[] =
    FILE_PATH_LITERAL("/home");

// Add a newly created LXD container to the kCrostiniContainers pref
void AddNewLxdContainerToPrefs(Profile* profile,
                               std::string vm_name,
                               std::string container_name);

// Remove a newly deleted LXD container from the kCrostiniContainers pref, and
// deregister its apps and mime types.
void RemoveLxdContainerFromPrefs(Profile* profile,
                                 std::string vm_name,
                                 std::string container_name);

// Returns a string to be displayed in a notification with the estimated time
// left for an operation to run which started and time |start| and is current
// at |percent| way through.
base::string16 GetTimeRemainingMessage(base::TimeTicks start, int percent);

// Splits the range between |min_size| and |available_space| into enough
// evenly-spaced intervals you can use them as ticks on a slider. Will return an
// empty set if the range is invalid (e.g. any numbers are negative).
// The number of ticks will fit in a signed integer.
std::vector<int64_t> GetTicksForDiskSize(int64_t min_size,
                                         int64_t available_space);

// Returns a pref value stored for a specific container.
const base::Value* GetContainerPrefValue(Profile* profile,
                                         const ContainerId& container_id,
                                         const std::string& key);

// Sets a pref value for a specific container.
void UpdateContainerPref(Profile* profile,
                         const ContainerId& container_id,
                         const std::string& key,
                         base::Value value);

const ContainerId& DefaultContainerId();

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
