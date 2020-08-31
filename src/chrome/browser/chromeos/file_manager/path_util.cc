// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/chromeos/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/smb_client/smbfs_share.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/drive/file_system_core_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace file_manager {
namespace util {

namespace {

constexpr char kAndroidFilesMountPointName[] = "android_files";
constexpr char kCrostiniMapGoogleDrive[] = "GoogleDrive";
constexpr char kCrostiniMapMyDrive[] = "MyDrive";
constexpr char kCrostiniMapPlayFiles[] = "PlayFiles";
constexpr char kCrostiniMapTeamDrives[] = "SharedDrives";
constexpr char kFolderNameDownloads[] = "Downloads";
constexpr char kFolderNameMyFiles[] = "MyFiles";
constexpr char kDisplayNameGoogleDrive[] = "Google Drive";
constexpr char kDriveFsDirComputers[] = "Computers";
constexpr char kDriveFsDirRoot[] = "root";
constexpr char kDriveFsDirTeamDrives[] = "team_drives";

// Sync with the root name defined with the file provider in ARC++ side.
constexpr base::FilePath::CharType kArcDownloadRoot[] =
    FILE_PATH_LITERAL("/download");
constexpr base::FilePath::CharType kArcExternalFilesRoot[] =
    FILE_PATH_LITERAL("/external_files");
// Sync with the volume provider in ARC++ side.
constexpr char kArcRemovableMediaContentUrlPrefix[] =
    "content://org.chromium.arc.volumeprovider/removable/";
constexpr char kArcMyFilesContentUrlPrefix[] =
    "content://org.chromium.arc.volumeprovider/MyFiles/";

Profile* GetPrimaryProfile() {
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  return chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

// Helper function for |ConvertToContentUrls|.
void OnSingleContentUrlResolved(const base::RepeatingClosure& barrier_closure,
                                std::vector<GURL>* out_urls,
                                size_t index,
                                const GURL& url) {
  (*out_urls)[index] = url;
  barrier_closure.Run();
}

// Helper function for |ConvertToContentUrls|.
void OnAllContentUrlsResolved(ConvertToContentUrlsCallback callback,
                              std::unique_ptr<std::vector<GURL>> urls) {
  std::move(callback).Run(*urls);
}

// On non-ChromeOS system (test+development), the primary profile uses
// $HOME/Downloads for ease access to local files for debugging.
bool ShouldMountPrimaryUserDownloads(Profile* profile) {
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      user_manager::UserManager::IsInitialized()) {
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            profile->GetOriginalProfile());
    const user_manager::User* const primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    return user == primary_user;
  }

  return false;
}

// Extracts the Drive path from the given path located under the legacy Drive
// mount point. Returns an empty path if |path| is not under the legacy Drive
// mount point.
// Example: ExtractLegacyDrivePath("/special/drive-xxx/foo.txt") =>
//   "drive/foo.txt"
base::FilePath ExtractLegacyDrivePath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.size() < 3)
    return base::FilePath();
  if (components[0] != FILE_PATH_LITERAL("/"))
    return base::FilePath();
  if (components[1] != FILE_PATH_LITERAL("special"))
    return base::FilePath();
  static const base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("drive");
  if (components[2].compare(0, base::size(kPrefix) - 1, kPrefix) != 0)
    return base::FilePath();

  base::FilePath drive_path = drive::util::GetDriveGrandRootPath();
  for (size_t i = 3; i < components.size(); ++i)
    drive_path = drive_path.Append(components[i]);
  return drive_path;
}

}  // namespace

const base::FilePath::CharType kRemovableMediaPath[] =
    FILE_PATH_LITERAL("/media/removable");

const base::FilePath::CharType kAndroidFilesPath[] =
    FILE_PATH_LITERAL("/run/arc/sdcard/write/emulated/0");

const base::FilePath::CharType kSystemFontsPath[] =
    FILE_PATH_LITERAL("/usr/share/fonts");

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // Check if FilesApp has a registered path already.  This happens for tests.
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path))
    return path.AppendASCII(kFolderNameDownloads);

  // Return $HOME/Downloads as Download folder.
  if (ShouldMountPrimaryUserDownloads(profile))
    return DownloadPrefs::GetDefaultDownloadDirectory();

  // Return <cryptohome>/MyFiles/Downloads if it feature is enabled.
  return profile->GetPath()
      .AppendASCII(kFolderNameMyFiles)
      .AppendASCII(kFolderNameDownloads);
}

base::FilePath GetMyFilesFolderForProfile(Profile* profile) {
  // Check if FilesApp has a registered path already. This happens for tests.
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path))
    return path;

  // Return $HOME/Downloads as MyFiles folder.
  if (ShouldMountPrimaryUserDownloads(profile))
    return DownloadPrefs::GetDefaultDownloadDirectory();

  // Return <cryptohome>/MyFiles.
  return profile->GetPath().AppendASCII(kFolderNameMyFiles);
}

base::FilePath GetAndroidFilesPath() {
  // Check if Android has a registered path already. This happens for tests.
  const std::string mount_point_name = util::GetAndroidFilesMountPointName();
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path))
    return path;
  return base::FilePath(file_manager::util::kAndroidFilesPath);
}

bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_base,
                              const base::FilePath& old_path,
                              base::FilePath* new_path) {
  const base::FilePath new_base = GetMyFilesFolderForProfile(profile);

  // Special case, migrating /home/chronos/user which is set early (before a
  // profile is attached to the browser process) to default to
  // /home/chronos/u-{hash}/MyFiles/Downloads.
  if (old_path == old_base &&
      old_path == base::FilePath("/home/chronos/user")) {
    *new_path = GetDownloadsFolderForProfile(profile);
    return true;
  }

  base::FilePath relative;
  if (old_base.AppendRelativePath(old_path, &relative)) {
    *new_path = new_base.Append(relative);
    return old_path != *new_path;
  }

  return false;
}

bool MigrateFromDownloadsToMyFiles(Profile* profile,
                                   const base::FilePath& old_path,
                                   base::FilePath* new_path) {
  const base::FilePath old_base =
      profile->GetPath().Append(kFolderNameDownloads);
  const base::FilePath new_base = GetDownloadsFolderForProfile(profile);
  if (new_base == old_base)
    return false;
  base::FilePath relative;
  if (old_path == old_base ||
      old_base.AppendRelativePath(old_path, &relative)) {
    *new_path = new_base.Append(relative);
    return old_path != *new_path;
  }
  return false;
}

bool MigrateToDriveFs(Profile* profile,
                      const base::FilePath& old_path,
                      base::FilePath* new_path) {
  const auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->is_enabled() || !user ||
      !user->GetAccountId().HasAccountIdKey()) {
    return false;
  }
  *new_path = integration_service->GetMountPointPath();
  return drive::util::GetDriveGrandRootPath().AppendRelativePath(
      ExtractLegacyDrivePath(old_path), new_path);
}

std::string GetDownloadsMountPointName(Profile* profile) {
  // To distinguish profiles in multi-profile session, we append user name hash
  // to "Downloads". Note that some profiles (like login or test profiles)
  // are not associated with an user account. In that case, no suffix is added
  // because such a profile never belongs to a multi-profile session.
  const user_manager::User* const user =
      user_manager::UserManager::IsInitialized()
          ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : nullptr;
  const std::string id = user ? "-" + user->username_hash() : "";
  return net::EscapeQueryParamValue(kFolderNameDownloads + id, false);
}

const std::string GetAndroidFilesMountPointName() {
  return kAndroidFilesMountPointName;
}

std::string GetCrostiniMountPointName(Profile* profile) {
  // crostini_<hash>_termina_penguin
  return base::JoinString(
      {"crostini", crostini::CryptohomeIdForProfile(profile),
       crostini::kCrostiniDefaultVmName,
       crostini::kCrostiniDefaultContainerName},
      "_");
}

base::FilePath GetCrostiniMountDirectory(Profile* profile) {
  return base::FilePath("/media/fuse/" + GetCrostiniMountPointName(profile));
}

std::vector<std::string> GetCrostiniMountOptions(
    const std::string& hostname,
    const std::string& host_private_key,
    const std::string& container_public_key) {
  const std::string port = "2222";
  std::vector<std::string> options;
  std::string base64_known_hosts;
  std::string base64_identity;
  base::Base64Encode(host_private_key, &base64_identity);
  base::Base64Encode(
      base::StringPrintf("[%s]:%s %s", hostname.c_str(), port.c_str(),
                         container_public_key.c_str()),
      &base64_known_hosts);
  options.push_back("UserKnownHostsBase64=" + base64_known_hosts);
  options.push_back("IdentityBase64=" + base64_identity);
  options.push_back("Port=" + port);
  return options;
}

bool ConvertFileSystemURLToPathInsideCrostini(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    base::FilePath* inside) {
  const std::string& id(file_system_url.mount_filesystem_id());
  // File system root requires strip trailing separator.
  base::FilePath path =
      base::FilePath(file_system_url.virtual_path()).StripTrailingSeparators();
  // Include drive if using DriveFS.
  std::string mount_point_name_drive;
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service) {
    mount_point_name_drive =
        integration_service->GetMountPointPath().BaseName().value();
  }

  // Reformat virtual_path() from:
  //   <mount_label>/path/to/file
  // To either:
  //   /<home-directory>/path/to/file   (path is already in crostini volume)
  //   /mnt/chromeos/<mapping>/path/to/file (path is shared with crostini)
  base::FilePath base_to_exclude(id);
  if (id == GetCrostiniMountPointName(profile)) {
    // Crostini.
    base::Optional<crostini::ContainerInfo> container_info =
        crostini::CrostiniManager::GetForProfile(profile)->GetContainerInfo(
            crostini::kCrostiniDefaultVmName,
            crostini::kCrostiniDefaultContainerName);
    if (!container_info) {
      return false;
    }
    *inside = container_info->homedir;
  } else if (id == GetDownloadsMountPointName(profile)) {
    // MyFiles.
    *inside =
        crostini::ContainerChromeOSBaseDirectory().Append(kFolderNameMyFiles);
  } else if (!mount_point_name_drive.empty() && id == mount_point_name_drive) {
    // DriveFS has some more complicated mappings.
    std::vector<base::FilePath::StringType> components;
    path.GetComponents(&components);
    *inside = crostini::ContainerChromeOSBaseDirectory().Append(
        kCrostiniMapGoogleDrive);
    if (components.size() >= 2 && components[1] == kDriveFsDirRoot) {
      // root -> MyDrive.
      base_to_exclude = base_to_exclude.Append(kDriveFsDirRoot);
      *inside = inside->Append(kCrostiniMapMyDrive);
    } else if (components.size() >= 2 &&
               components[1] == kDriveFsDirTeamDrives) {
      // team_drives -> SharedDrives.
      base_to_exclude = base_to_exclude.Append(kDriveFsDirTeamDrives);
      *inside = inside->Append(kCrostiniMapTeamDrives);
    }
  } else if (id == chromeos::kSystemMountNameRemovable) {
    // Removable.
    *inside = crostini::ContainerChromeOSBaseDirectory().Append(
        chromeos::kSystemMountNameRemovable);
  } else if (id == GetAndroidFilesMountPointName()) {
    *inside = crostini::ContainerChromeOSBaseDirectory().Append(
        kCrostiniMapPlayFiles);
  } else {
    return false;
  }
  return base_to_exclude == path ||
         base_to_exclude.AppendRelativePath(path, inside);
}

bool ConvertPathToArcUrl(const base::FilePath& path, GURL* arc_url_out) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Obtain the primary profile. This information is required because currently
  // only the file systems for the primary profile is exposed to ARC.
  Profile* primary_profile = GetPrimaryProfile();
  if (!primary_profile)
    return false;

  // Convert paths under primary profile's Downloads directory.
  base::FilePath primary_downloads =
      GetDownloadsFolderForProfile(primary_profile);
  base::FilePath result_path(kArcDownloadRoot);
  if (primary_downloads.AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(arc::kFileSystemFileproviderUrl)
                       .Resolve(net::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under Android files root (/run/arc/sdcard/write/emulated/0).
  result_path = base::FilePath(kArcExternalFilesRoot);
  if (base::FilePath(kAndroidFilesPath)
          .AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(arc::kFileSystemFileproviderUrl)
                       .Resolve(net::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under /media/removable.
  base::FilePath relative_path;
  if (base::FilePath(kRemovableMediaPath)
          .AppendRelativePath(path, &relative_path)) {
    *arc_url_out = GURL(kArcRemovableMediaContentUrlPrefix)
                       .Resolve(net::EscapePath(relative_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under MyFiles
  if (base::FilePath(GetMyFilesFolderForProfile(primary_profile))
          .AppendRelativePath(path, &relative_path)) {
    *arc_url_out = GURL(kArcMyFilesContentUrlPrefix)
                       .Resolve(net::EscapePath(relative_path.AsUTF8Unsafe()));
    return true;
  }

  bool force_external = false;
  // Force external URL for DriveFS, Crostini and smbfs.
  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(primary_profile);
  if ((integration_service &&
       integration_service->GetMountPointPath().AppendRelativePath(
           path, &relative_path)) ||
      GetCrostiniMountDirectory(primary_profile)
          .AppendRelativePath(path, &relative_path)) {
    force_external = true;
  }

  chromeos::smb_client::SmbService* smb_service =
      chromeos::smb_client::SmbServiceFactory::Get(primary_profile);
  if (smb_service) {
    chromeos::smb_client::SmbFsShare* share =
        smb_service->GetSmbFsShareForPath(path);
    if (share && share->mount_path().AppendRelativePath(path, &relative_path)) {
      force_external = true;
    }
  }

  // Convert paths under /special or other paths forced to use external URL.
  GURL external_file_url = chromeos::CreateExternalFileURLFromPath(
      primary_profile, path, force_external);

  if (!external_file_url.is_empty()) {
    *arc_url_out = arc::EncodeToChromeContentProviderUrl(external_file_url);
    return true;
  }

  // TODO(kinaba): Add conversion logic once other file systems are supported.
  return false;
}

void ConvertToContentUrls(
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (file_system_urls.empty()) {
    std::move(callback).Run(std::vector<GURL>());
    return;
  }

  Profile* profile = GetPrimaryProfile();
  auto* documents_provider_root_map =
      profile ? arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile)
              : nullptr;

  // To keep the original order, prefill |out_urls| with empty URLs and
  // specify index when updating it like (*out_urls)[index] = url.
  auto out_urls = std::make_unique<std::vector<GURL>>(file_system_urls.size());
  auto* out_urls_ptr = out_urls.get();
  auto barrier = base::BarrierClosure(
      file_system_urls.size(),
      base::BindOnce(&OnAllContentUrlsResolved, std::move(callback),
                     std::move(out_urls)));
  auto single_content_url_callback =
      base::BindRepeating(&OnSingleContentUrlResolved, barrier, out_urls_ptr);

  for (size_t index = 0; index < file_system_urls.size(); ++index) {
    const auto& file_system_url = file_system_urls[index];

    // Run DocumentsProvider check before running ConvertPathToArcUrl.
    // Otherwise, DocumentsProvider file path would be encoded to a
    // ChromeContentProvider URL (b/132314050).
    if (documents_provider_root_map) {
      base::FilePath file_path;
      auto* documents_provider_root =
          documents_provider_root_map->ParseAndLookup(file_system_url,
                                                      &file_path);
      if (documents_provider_root) {
        documents_provider_root->ResolveToContentUrl(
            file_path, base::BindOnce(single_content_url_callback, index));
        continue;
      }
    }

    GURL arc_url;
    if (file_system_url.mount_type() == storage::kFileSystemTypeExternal &&
        ConvertPathToArcUrl(file_system_url.path(), &arc_url)) {
      single_content_url_callback.Run(index, arc_url);
      continue;
    }

    single_content_url_callback.Run(index, GURL());
  }
}

bool ReplacePrefix(std::string* s,
                   const std::string& prefix,
                   const std::string& replacement) {
  if (base::StartsWith(*s, prefix, base::CompareCase::SENSITIVE)) {
    base::ReplaceFirstSubstringAfterOffset(s, 0, prefix, replacement);
    return true;
  }
  return false;
}

std::string GetPathDisplayTextForSettings(Profile* profile,
                                          const std::string& path) {
  std::string result(path);
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (drive_integration_service && !drive_integration_service->is_enabled()) {
    drive_integration_service = nullptr;
  }
  if (ReplacePrefix(&result, "/home/chronos/user/Downloads",
                    kFolderNameDownloads)) {
  } else if (ReplacePrefix(&result,
                           "/home/chronos/" +
                               profile->GetPath().BaseName().value() +
                               "/Downloads",
                           kFolderNameDownloads)) {
  } else if (ReplacePrefix(
                 &result,
                 std::string("/home/chronos/user/") + kFolderNameMyFiles,
                 "My files")) {
  } else if (ReplacePrefix(&result,
                           "/home/chronos/" +
                               profile->GetPath().BaseName().value() + "/" +
                               kFolderNameMyFiles,
                           "My files")) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirRoot)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL))
                               .value())) {
  } else if (ReplacePrefix(&result,
                           download_dir_util::kDriveNamePolicyVariableName,
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL))
                               .value())) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirTeamDrives)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_SHARED_DRIVES_LABEL))
                               .value())) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirComputers)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_COMPUTERS_LABEL))
                               .value())) {
  } else if (ReplacePrefix(&result, kAndroidFilesPath,
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL))) {
  } else if (ReplacePrefix(&result, GetCrostiniMountDirectory(profile).value(),
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL))) {
  } else if (ReplacePrefix(&result,
                           base::FilePath(kRemovableMediaPath)
                               .AsEndingWithSeparator()
                               .value(),
                           "")) {
    // Strip prefix of "/media/removable/" including trailing slash.
  }

  base::ReplaceChars(result, "/", " \u203a ", &result);
  return result;
}

bool ExtractMountNameFileSystemNameFullPath(const base::FilePath& absolute_path,
                                            std::string* mount_name,
                                            std::string* file_system_name,
                                            std::string* full_path) {
  DCHECK(absolute_path.IsAbsolute());
  DCHECK(mount_name);
  DCHECK(full_path);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  if (!mount_points->GetVirtualPath(absolute_path, &virtual_path))
    return false;
  // |virtual_path| format is: <mount_name>/<full_path>, and
  // |file_system_name| == |mount_name|, except for 'removable' and 'archive',
  // |mount_name| is the first two segments, |file_system_name| is the second.
  const std::string& value = virtual_path.value();
  size_t fs_start = 0;
  size_t slash_pos = value.find(base::FilePath::kSeparators[0]);
  *mount_name = *file_system_name = value.substr(0, slash_pos);
  if (*mount_name == chromeos::kSystemMountNameRemovable ||
      *mount_name == chromeos::kSystemMountNameArchive) {
    if (slash_pos == std::string::npos) {
      return false;
    }
    fs_start = slash_pos + 1;
    slash_pos = value.find(base::FilePath::kSeparators[0], fs_start);
    *mount_name = value.substr(0, slash_pos);
  }

  // Set full_path to '/' if |absolute_path| is a root.
  if (slash_pos == std::string::npos) {
    *file_system_name = value.substr(fs_start);
    *full_path = "/";
  } else {
    *file_system_name = value.substr(fs_start, slash_pos - fs_start);
    *full_path = value.substr(slash_pos);
  }
  return true;
}

}  // namespace util
}  // namespace file_manager
