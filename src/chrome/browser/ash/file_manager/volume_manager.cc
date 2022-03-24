// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "ash/components/disks/disk.h"
#include "ash/components/disks/disk_mount_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/snapshot_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace file_manager {
namespace {

const uint32_t kAccessCapabilityReadWrite = 0;
const uint32_t kFilesystemTypeGenericHierarchical = 2;
const char kFileManagerMTPMountNamePrefix[] = "fileman-mtp-";
const char kMtpVolumeIdPrefix[] = "mtp:";
const char kRootPath[] = "/";

// Registers |path| as the "Downloads" folder to the FileSystem API backend.
// If another folder is already mounted. It revokes and overrides the old one.
bool RegisterDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  // Although we show only profile's own "Downloads" folder in the Files app,
  // in the backend we need to mount all profile's download directory globally.
  // Otherwise, the Files app cannot support cross-profile file copies, etc.
  // For this reason, we need to register to the global GetSystemInstance().
  const std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // In some tests we want to override existing Downloads mount point, so we
  // first revoke the existing mount point (if any).
  mount_points->RevokeFileSystem(mount_point_name);
  return mount_points->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), path);
}

// Registers a mount point for Android files to ExternalMountPoints.
bool RegisterAndroidFilesMountPoint() {
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      base::FilePath(util::kAndroidFilesPath));
}

bool RegisterShareCacheMountPoint(Profile* profile) {
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->RegisterFileSystem(
      util::kShareCacheMountPointName, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), util::GetShareCacheFilePath(profile));
}

// Finds the path register as the "Downloads" folder to FileSystem API backend.
// Returns false if it is not registered.
bool FindDownloadsMountPointPath(Profile* profile, base::FilePath* path) {
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  return mount_points->GetRegisteredPath(mount_point_name, path);
}

VolumeType MountTypeToVolumeType(chromeos::MountType type) {
  switch (type) {
    case chromeos::MOUNT_TYPE_INVALID:
      // We don't expect this value, but list here, so that when any value
      // is added to the enum definition but this is not edited, the compiler
      // warns it.
      break;
    case chromeos::MOUNT_TYPE_DEVICE:
      return VOLUME_TYPE_REMOVABLE_DISK_PARTITION;
    case chromeos::MOUNT_TYPE_ARCHIVE:
      return VOLUME_TYPE_MOUNTED_ARCHIVE_FILE;
    case chromeos::MOUNT_TYPE_NETWORK_STORAGE:
      // Network storage mounts are handled by their mounters so
      // MOUNT_TYPE_NETWORK_STORAGE should never need to be handled here.
      break;
  }

  NOTREACHED();
  return VOLUME_TYPE_DOWNLOADS_DIRECTORY;
}

// Returns a string representation of the given volume type.
std::string VolumeTypeToString(VolumeType type) {
  switch (type) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      return "drive";
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      return "downloads";
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return "removable";
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      return "archive";
    case VOLUME_TYPE_PROVIDED:
      return "provided";
    case VOLUME_TYPE_MTP:
      return "mtp";
    case VOLUME_TYPE_MEDIA_VIEW:
      return "media_view";
    case VOLUME_TYPE_ANDROID_FILES:
      return "android_files";
    case VOLUME_TYPE_DOCUMENTS_PROVIDER:
      return "documents_provider";
    case VOLUME_TYPE_TESTING:
      return "testing";
    case VOLUME_TYPE_CROSTINI:
      return "crostini";
    case VOLUME_TYPE_SMB:
      return "smb";
    case VOLUME_TYPE_SYSTEM_INTERNAL:
      return "system_internal";
    case NUM_VOLUME_TYPE:
      break;
  }
  NOTREACHED();
  return "";
}

// Generates a unique volume ID for the given volume info.
std::string GenerateVolumeId(const Volume& volume) {
  // For the same volume type, base names are unique, as mount points are
  // flat for the same volume type.
  return (VolumeTypeToString(volume.type()) + ":" +
          volume.mount_path().BaseName().AsUTF8Unsafe());
}

std::string GetMountPointNameForMediaStorage(
    const storage_monitor::StorageInfo& info) {
  std::string name(kFileManagerMTPMountNamePrefix);
  name += info.device_id();
  return name;
}

chromeos::MountAccessMode GetExternalStorageAccessMode(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)
             ? chromeos::MOUNT_ACCESS_MODE_READ_ONLY
             : chromeos::MOUNT_ACCESS_MODE_READ_WRITE;
}

void RecordDownloadsDiskUsageStats(base::FilePath downloads_path) {
  constexpr int64_t kOneMiB = 1024 * 1024;
  // For now assume a maximum bucket size of 512GB, which exceeds all current
  // chromeOS hard disk sizes.
  constexpr int64_t k512GiBInMiB = 512 * 1024;

  int64_t download_directory_size_in_bytes =
      base::ComputeDirectorySize(downloads_path);

  base::UmaHistogramCustomCounts(
      "FileBrowser.Downloads.DirectorySizeMiB",
      static_cast<int>(download_directory_size_in_bytes / kOneMiB), 1,
      k512GiBInMiB, 100);

  int64_t total_disk_space_in_bytes =
      base::SysInfo::AmountOfTotalDiskSpace(downloads_path);

  // total_disk_space_in_bytes can be -1 on error.
  if (total_disk_space_in_bytes > 0) {
    int percentage_space_used = std::lround(
        (download_directory_size_in_bytes * 100.0) / total_disk_space_in_bytes);

    base::UmaHistogramPercentageObsoleteDoNotUse(
        "FileBrowser.Downloads.DirectoryPercentageOfDiskUsage",
        percentage_space_used);
  }
}

}  // namespace

Volume::Volume()
    : source_(SOURCE_FILE),
      type_(VOLUME_TYPE_GOOGLE_DRIVE),
      device_type_(chromeos::DEVICE_TYPE_UNKNOWN),
      mount_condition_(ash::disks::MOUNT_CONDITION_NONE),
      mount_context_(MOUNT_CONTEXT_UNKNOWN),
      is_parent_(false),
      is_read_only_(false),
      is_read_only_removable_device_(false),
      has_media_(false),
      configurable_(false),
      watchable_(false),
      hidden_(false) {}

Volume::~Volume() = default;

// static
std::unique_ptr<Volume> Volume::CreateForDrive(
    const base::FilePath& drive_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_GOOGLE_DRIVE;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  volume->source_path_ = drive_path;
  volume->source_ = SOURCE_NETWORK;
  volume->mount_path_ = drive_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForDownloads(
    const base::FilePath& downloads_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = downloads_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForRemovable(
    const ash::disks::DiskMountManager::MountPointInfo& mount_point,
    const ash::disks::Disk* disk) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = MountTypeToVolumeType(mount_point.mount_type);
  volume->source_path_ = base::FilePath(mount_point.source_path);
  volume->source_ = mount_point.mount_type == chromeos::MOUNT_TYPE_ARCHIVE
                        ? SOURCE_FILE
                        : SOURCE_DEVICE;
  volume->mount_path_ = base::FilePath(mount_point.mount_path);
  volume->mount_condition_ = mount_point.mount_condition;

  if (disk) {
    volume->file_system_type_ = disk->file_system_type();
    volume->volume_label_ = disk->device_label();
    volume->device_type_ = disk->device_type();
    volume->storage_device_path_ = base::FilePath(disk->storage_device_path());
    volume->is_parent_ = disk->is_parent();
    volume->is_read_only_ = disk->is_read_only();
    volume->is_read_only_removable_device_ = disk->is_read_only_hardware();
    volume->has_media_ = disk->has_media();
    volume->drive_label_ = disk->drive_label();
  } else {
    volume->volume_label_ = volume->mount_path().BaseName().AsUTF8Unsafe();
    volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
    volume->is_read_only_ =
        (mount_point.mount_type == chromeos::MOUNT_TYPE_ARCHIVE);
  }
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForProvidedFileSystem(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    MountContext mount_context) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->file_system_id_ = file_system_info.file_system_id();
  volume->provider_id_ = file_system_info.provider_id();
  switch (file_system_info.source()) {
    case extensions::SOURCE_FILE:
      volume->source_ = SOURCE_FILE;
      break;
    case extensions::SOURCE_DEVICE:
      volume->source_ = SOURCE_DEVICE;
      break;
    case extensions::SOURCE_NETWORK:
      volume->source_ = SOURCE_NETWORK;
      break;
  }
  volume->volume_label_ = file_system_info.display_name();
  volume->type_ = VOLUME_TYPE_PROVIDED;
  volume->mount_path_ = file_system_info.mount_path();
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->mount_context_ = mount_context;
  volume->is_parent_ = true;
  volume->is_read_only_ = !file_system_info.writable();
  volume->configurable_ = file_system_info.configurable();
  volume->watchable_ = file_system_info.watchable();
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->icon_set_ = file_system_info.icon_set();
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForMTP(const base::FilePath& mount_path,
                                             const std::string& label,
                                             bool read_only) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_MTP;
  volume->mount_path_ = mount_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->is_parent_ = true;
  volume->is_read_only_ = read_only;
  volume->volume_id_ = kMtpVolumeIdPrefix + label;
  volume->volume_label_ = label;
  volume->source_path_ = mount_path;
  volume->source_ = SOURCE_DEVICE;
  volume->device_type_ = chromeos::DEVICE_TYPE_MOBILE;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForMediaView(
    const std::string& root_document_id) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_MEDIA_VIEW;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = arc::GetDocumentsProviderMountPath(
      arc::kMediaDocumentsProviderAuthority, root_document_id);
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_label_ = root_document_id;
  volume->is_read_only_ = true;
  volume->watchable_ = false;
  volume->volume_id_ = arc::GetMediaViewVolumeId(root_document_id);
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForSshfsCrostini(
    const base::FilePath& sshfs_mount_path,
    const base::FilePath& remote_mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_CROSTINI;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = sshfs_mount_path;
  volume->remote_mount_path_ = remote_mount_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = false;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForAndroidFiles(
    const base::FilePath& mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_ANDROID_FILES;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = mount_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForDocumentsProvider(
    const std::string& authority,
    const std::string& root_id,
    const std::string& document_id,
    const std::string& title,
    const std::string& summary,
    const GURL& icon_url,
    bool read_only) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_DOCUMENTS_PROVIDER;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ =
      arc::GetDocumentsProviderMountPath(authority, document_id);
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_label_ = title;
  volume->is_read_only_ = read_only;
  volume->watchable_ = false;
  volume->volume_id_ = arc::GetDocumentsProviderVolumeId(authority, root_id);
  if (!icon_url.is_empty()) {
    ash::file_system_provider::IconSet icon_set;
    icon_set.SetIcon(ash::file_system_provider::IconSet::IconSize::SIZE_32x32,
                     icon_url);
    volume->icon_set_ = icon_set;
  }
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForSmb(const base::FilePath& mount_point,
                                             const std::string display_name) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_SMB;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_NETWORK;
  volume->mount_path_ = mount_point;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ = display_name;
  volume->watchable_ = false;
  volume->is_read_only_ = false;
  return volume;
}

// ShareCache is not visible in the file manager and so this volume does not
// represent a real, user-visible Volume. However, shared files can be read
// through ImageLoader, which needs a Volume present to be able to read from the
// directory.
// static
std::unique_ptr<Volume> Volume::CreateForShareCache(
    const base::FilePath& mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_SYSTEM_INTERNAL;
  volume->device_type_ = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = mount_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = false;
  volume->is_read_only_ = true;
  volume->hidden_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForTesting(
    const base::FilePath& path,
    VolumeType volume_type,
    chromeos::DeviceType device_type,
    bool read_only,
    const base::FilePath& device_path,
    const std::string& drive_label,
    const std::string& file_system_type,
    bool hidden) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = volume_type;
  volume->device_type_ = device_type;
  // Keep source_path empty.
  volume->source_ = SOURCE_DEVICE;
  volume->mount_path_ = path;
  volume->storage_device_path_ = device_path;
  volume->mount_condition_ = ash::disks::MOUNT_CONDITION_NONE;
  volume->is_read_only_ = read_only;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->drive_label_ = drive_label;
  if (volume_type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    volume->file_system_type_ = file_system_type;
  }
  volume->hidden_ = hidden;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForTesting(
    const base::FilePath& device_path,
    const base::FilePath& mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->storage_device_path_ = device_path;
  volume->mount_path_ = mount_path;
  return volume;
}

VolumeManager::VolumeManager(
    Profile* profile,
    drive::DriveIntegrationService* drive_integration_service,
    chromeos::PowerManagerClient* power_manager_client,
    ash::disks::DiskMountManager* disk_mount_manager,
    ash::file_system_provider::Service* file_system_provider_service,
    GetMtpStorageInfoCallback get_mtp_storage_info_callback)
    : profile_(profile),
      drive_integration_service_(drive_integration_service),
      disk_mount_manager_(disk_mount_manager),
      file_system_provider_service_(file_system_provider_service),
      get_mtp_storage_info_callback_(get_mtp_storage_info_callback),
      snapshot_manager_(new SnapshotManager(profile_)),
      documents_provider_root_manager_(
          std::make_unique<DocumentsProviderRootManager>(
              profile_,
              arc::ArcFileSystemOperationRunner::GetForBrowserContext(
                  profile_))),
      io_task_controller_() {
  DCHECK(disk_mount_manager);
}

VolumeManager::~VolumeManager() = default;

VolumeManager* VolumeManager::Get(content::BrowserContext* context) {
  return VolumeManagerFactory::Get(context);
}

void VolumeManager::Initialize() {
  // If in the Sign in profile or the lock screen app profile or lock screen
  // profile, skip mounting and listening for mount events.
  if (!chromeos::ProfileHelper::IsRegularProfile(profile_)) {
    return;
  }

  const base::FilePath localVolume =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  const bool success = RegisterDownloadsMountPoint(profile_, localVolume);
  DCHECK(success);

  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               Volume::CreateForDownloads(localVolume));

  // Asyncrhonously record the disk usage for the downloads path
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RecordDownloadsDiskUsageStats, std::move(localVolume)));

  // Subscribe to DriveIntegrationService.
  drive_integration_service_->AddObserver(this);
  if (drive_integration_service_->IsMounted()) {
    DoMountEvent(chromeos::MOUNT_ERROR_NONE,
                 Volume::CreateForDrive(GetDriveMountPointPath()));
  }

  // Subscribe to DiskMountManager.
  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->EnsureMountInfoRefreshed(
      base::BindOnce(&VolumeManager::OnDiskMountManagerRefreshed,
                     weak_ptr_factory_.GetWeakPtr()),
      false /* force */);

  // Subscribe to ARC DocumentsProvider events about roots.
  documents_provider_root_manager_->AddObserver(this);

  // Subscribe to FileSystemProviderService and register currently mounted
  // volumes for the profile.
  if (file_system_provider_service_) {
    using ash::file_system_provider::ProvidedFileSystemInfo;
    file_system_provider_service_->AddObserver(this);

    std::vector<ProvidedFileSystemInfo> file_system_info_list =
        file_system_provider_service_->GetProvidedFileSystemInfoList();
    for (size_t i = 0; i < file_system_info_list.size(); ++i) {
      std::unique_ptr<Volume> volume = Volume::CreateForProvidedFileSystem(
          file_system_info_list[i], MOUNT_CONTEXT_AUTO);
      DoMountEvent(chromeos::MOUNT_ERROR_NONE, std::move(volume));
    }
  }

  // Subscribe to Profile Preference change.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kExternalStorageDisabled,
      base::BindRepeating(&VolumeManager::OnExternalStorageDisabledChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kExternalStorageReadOnly,
      base::BindRepeating(&VolumeManager::OnExternalStorageReadOnlyChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Subscribe to storage monitor for MTP notifications.
  if (storage_monitor::StorageMonitor::GetInstance()) {
    storage_monitor::StorageMonitor::GetInstance()->EnsureInitialized(
        base::BindOnce(&VolumeManager::OnStorageMonitorInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Subscribe to ARC file system events.
  if (base::FeatureList::IsEnabled(arc::kMediaViewFeature) &&
      arc::IsArcAllowedForProfile(profile_)) {
    // Registers a mount point for Android files only when the flag is enabled.
    RegisterAndroidFilesMountPoint();

    arc::ArcSessionManager::Get()->AddObserver(this);
    OnArcPlayStoreEnabledChanged(
        arc::IsArcPlayStoreEnabledForProfile(profile_));
  }

  RegisterShareCacheMountPoint(profile_);
  DoMountEvent(
      chromeos::MOUNT_ERROR_NONE,
      Volume::CreateForShareCache(util::GetShareCacheFilePath(profile_)));
}

void VolumeManager::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  snapshot_manager_.reset();
  pref_change_registrar_.RemoveAll();
  disk_mount_manager_->RemoveObserver(this);
  documents_provider_root_manager_->RemoveObserver(this);
  documents_provider_root_manager_.reset();
  if (storage_monitor::StorageMonitor::GetInstance())
    storage_monitor::StorageMonitor::GetInstance()->RemoveObserver(this);

  if (drive_integration_service_)
    drive_integration_service_->RemoveObserver(this);

  if (file_system_provider_service_)
    file_system_provider_service_->RemoveObserver(this);

  // Unsubscribe from ARC file system events.
  if (base::FeatureList::IsEnabled(arc::kMediaViewFeature) &&
      arc::IsArcAllowedForProfile(profile_)) {
    auto* session_manager = arc::ArcSessionManager::Get();
    // TODO(crbug.com/672829): We need nullptr check here because
    // ArcSessionManager may or may not be alive at this point.
    if (session_manager)
      session_manager->RemoveObserver(this);
  }
}

void VolumeManager::AddObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void VolumeManager::RemoveObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::vector<base::WeakPtr<Volume>> VolumeManager::GetVolumeList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<base::WeakPtr<Volume>> result;
  result.reserve(mounted_volumes_.size());
  for (const auto& pair : mounted_volumes_) {
    result.push_back(pair.second->AsWeakPtr());
  }
  return result;
}

base::WeakPtr<Volume> VolumeManager::FindVolumeById(
    const std::string& volume_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto it = mounted_volumes_.find(volume_id);
  if (it != mounted_volumes_.end())
    return it->second->AsWeakPtr();
  return base::WeakPtr<Volume>();
}

base::WeakPtr<Volume> VolumeManager::FindVolumeFromPath(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& pair : mounted_volumes_) {
    base::FilePath volume_mount_path = pair.second->mount_path();
    if (path == volume_mount_path || volume_mount_path.IsParent(path)) {
      return pair.second->AsWeakPtr();
    }
  }
  return nullptr;
}

void VolumeManager::AddSshfsCrostiniVolume(
    const base::FilePath& sshfs_mount_path,
    const base::FilePath& remote_mount_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<Volume> volume =
      Volume::CreateForSshfsCrostini(sshfs_mount_path, remote_mount_path);
  // Ignore if volume already exists.
  if (mounted_volumes_.find(volume->volume_id()) != mounted_volumes_.end())
    return;
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, std::move(volume));

  // Listen for crostini container shutdown and remove volume.
  crostini::CrostiniManager::GetForProfile(profile_)
      ->AddShutdownContainerCallback(
          crostini::ContainerId::GetDefault(),
          base::BindOnce(&VolumeManager::RemoveSshfsCrostiniVolume,
                         weak_ptr_factory_.GetWeakPtr(), sshfs_mount_path,
                         base::BindOnce([](bool result) {
                           if (!result)
                             LOG(ERROR) << "Failed to remove sshfs mount";
                         })));
}

void VolumeManager::RemoveSshfsCrostiniVolume(
    const base::FilePath& sshfs_mount_path,
    RemoveSshfsCrostiniVolumeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  disk_mount_manager_->UnmountPath(
      sshfs_mount_path.value(),
      base::BindOnce(&VolumeManager::OnSshfsCrostiniUnmountCallback,
                     base::Unretained(this), sshfs_mount_path,
                     std::move(callback)));
}

bool VolumeManager::RegisterAndroidFilesDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool result =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetAndroidFilesMountPointName(),
          storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
          path);
  DCHECK(result);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, Volume::CreateForAndroidFiles(path));
  return true;
}

bool VolumeManager::RegisterMediaViewForTesting(
    const std::string& root_document_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               Volume::CreateForMediaView(root_document_id));
  return true;
}

bool VolumeManager::RemoveAndroidFilesDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                 *Volume::CreateForAndroidFiles(path));
  return true;
}

void VolumeManager::RemoveDownloadsDirectoryForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::FilePath path;
  if (FindDownloadsMountPointPath(profile_, &path)) {
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForDownloads(path));
  }
}

bool VolumeManager::RegisterDownloadsDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath old_path;
  if (FindDownloadsMountPointPath(profile_, &old_path)) {
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForDownloads(old_path));
  }

  bool success = RegisterDownloadsMountPoint(profile_, path);
  DoMountEvent(
      success ? chromeos::MOUNT_ERROR_NONE : chromeos::MOUNT_ERROR_INVALID_PATH,
      Volume::CreateForDownloads(path));
  return success;
}

bool VolumeManager::RegisterCrostiniDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool success =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetCrostiniMountPointName(profile_),
          storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
          path);
  DoMountEvent(
      success ? chromeos::MOUNT_ERROR_NONE : chromeos::MOUNT_ERROR_INVALID_PATH,
      Volume::CreateForSshfsCrostini(path, base::FilePath("/home/testuser")));
  return true;
}

void VolumeManager::AddVolumeForTesting(const base::FilePath& path,
                                        VolumeType volume_type,
                                        chromeos::DeviceType device_type,
                                        bool read_only,
                                        const base::FilePath& device_path,
                                        const std::string& drive_label,
                                        const std::string& file_system_type,
                                        bool hidden) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               Volume::CreateForTesting(path, volume_type, device_type,
                                        read_only, device_path, drive_label,
                                        file_system_type, hidden));
}

void VolumeManager::AddVolumeForTesting(std::unique_ptr<Volume> volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, std::move(volume));
}

void VolumeManager::RemoveVolumeForTesting(
    const base::FilePath& path,
    VolumeType volume_type,
    chromeos::DeviceType device_type,
    bool read_only,
    const base::FilePath& device_path,
    const std::string& drive_label,
    const std::string& file_system_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(
      chromeos::MOUNT_ERROR_NONE,
      *Volume::CreateForTesting(path, volume_type, device_type, read_only,
                                device_path, drive_label, file_system_type));
}

void VolumeManager::OnFileSystemMounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Raise mount event.
  // We can pass chromeos::MOUNT_ERROR_NONE even when authentication is failed
  // or network is unreachable. These two errors will be handled later.
  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               Volume::CreateForDrive(GetDriveMountPointPath()));
}

void VolumeManager::OnFileSystemBeingUnmounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                 *Volume::CreateForDrive(GetDriveMountPointPath()));
}

void VolumeManager::OnAutoMountableDiskEvent(
    ash::disks::DiskMountManager::DiskEvent event,
    const ash::disks::Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Disregard hidden devices.
  if (disk.is_hidden())
    return;

  switch (event) {
    case ash::disks::DiskMountManager::DISK_ADDED:
    case ash::disks::DiskMountManager::DISK_CHANGED: {
      if (disk.device_path().empty()) {
        DVLOG(1) << "Empty system path for " << disk.device_path();
        return;
      }

      bool mounting = false;
      if (disk.mount_path().empty() && disk.has_media() &&
          !profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
        // TODO(crbug.com/774890): Remove |mount_label| when the issue gets
        // resolved. Currently we suggest a mount point name, because in case
        // when disk's name contains '#', content will not load in Files App.
        std::string mount_label = disk.device_label();
        std::replace(mount_label.begin(), mount_label.end(), '#', '_');

        // If disk is not mounted yet and it has media and there is no policy
        // forbidding external storage, give it a try.
        // Initiate disk mount operation. MountPath auto-detects the filesystem
        // format if the second argument is empty. The third argument (mount
        // label) is not used in a disk mount operation.
        disk_mount_manager_->MountPath(
            disk.device_path(), std::string(), mount_label, {},
            chromeos::MOUNT_TYPE_DEVICE, GetExternalStorageAccessMode(profile_),
            base::DoNothing());
        mounting = true;
      }

      // Notify to observers.
      for (auto& observer : observers_)
        observer.OnDiskAdded(disk, mounting);
      return;
    }

    case ash::disks::DiskMountManager::DISK_REMOVED:
      // If the disk is already mounted, unmount it.
      if (!disk.mount_path().empty()) {
        disk_mount_manager_->UnmountPath(
            disk.mount_path(),
            ash::disks::DiskMountManager::UnmountPathCallback());
      }

      // Notify to observers.
      for (auto& observer : observers_)
        observer.OnDiskRemoved(disk);
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnDeviceEvent(
    ash::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "OnDeviceEvent: " << event << ", " << device_path;
  switch (event) {
    case ash::disks::DiskMountManager::DEVICE_ADDED:
      for (auto& observer : observers_)
        observer.OnDeviceAdded(device_path);
      return;
    case ash::disks::DiskMountManager::DEVICE_REMOVED: {
      for (auto& observer : observers_)
        observer.OnDeviceRemoved(device_path);
      return;
    }
    case ash::disks::DiskMountManager::DEVICE_SCANNED:
      DVLOG(1) << "Ignore SCANNED event: " << device_path;
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnMountEvent(
    ash::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const ash::disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Network storage is responsible for doing its own mounting.
  if (mount_info.mount_type == chromeos::MOUNT_TYPE_NETWORK_STORAGE)
    return;

  // Notify a mounting/unmounting event to observers.
  const ash::disks::Disk* const disk =
      disk_mount_manager_->FindDiskBySourcePath(mount_info.source_path);
  std::unique_ptr<Volume> volume = Volume::CreateForRemovable(mount_info, disk);
  switch (event) {
    case ash::disks::DiskMountManager::MOUNTING: {
      DoMountEvent(error_code, std::move(volume));
      return;
    }
    case ash::disks::DiskMountManager::UNMOUNTING:
      DoUnmountEvent(error_code, *volume);
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnFormatEvent(
    ash::disks::DiskMountManager::FormatEvent event,
    chromeos::FormatError error_code,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnDeviceEvent: " << event << ", " << error_code
           << ", " << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::FORMAT_STARTED:
      for (auto& observer : observers_) {
        observer.OnFormatStarted(device_path, device_label,
                                 error_code == chromeos::FORMAT_ERROR_NONE);
      }
      return;
    case ash::disks::DiskMountManager::FORMAT_COMPLETED:
      // Even if format did not complete successfully, try to mount the device
      // so the user can retry.
      // MountPath auto-detects filesystem format if second argument is
      // empty. The third argument (mount label) is not used in a disk mount
      // operation.
      disk_mount_manager_->MountPath(device_path, std::string(), std::string(),
                                     {}, chromeos::MOUNT_TYPE_DEVICE,
                                     GetExternalStorageAccessMode(profile_),
                                     base::DoNothing());

      for (auto& observer : observers_) {
        observer.OnFormatCompleted(device_path, device_label,
                                   error_code == chromeos::FORMAT_ERROR_NONE);
      }

      return;
  }
  NOTREACHED();
}

void VolumeManager::OnPartitionEvent(
    ash::disks::DiskMountManager::PartitionEvent event,
    chromeos::PartitionError error_code,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnPartitionEvent: " << event << ", " << error_code << ", "
           << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::PARTITION_STARTED:
      for (auto& observer : observers_) {
        observer.OnPartitionStarted(
            device_path, device_label,
            error_code == chromeos::PARTITION_ERROR_NONE);
      }
      return;
    case ash::disks::DiskMountManager::PARTITION_COMPLETED:
      // If partitioning failed, try to mount the device so the user can retry.
      // MountPath auto-detects filesystem format if second argument is
      // empty. The third argument (mount label) is not used in a disk mount
      // operation.
      if (error_code != chromeos::PARTITION_ERROR_NONE) {
        disk_mount_manager_->MountPath(
            device_path, std::string(), std::string(), {},
            chromeos::MOUNT_TYPE_DEVICE, GetExternalStorageAccessMode(profile_),
            base::DoNothing());
      }

      for (auto& observer : observers_) {
        observer.OnPartitionCompleted(
            device_path, device_label,
            error_code == chromeos::PARTITION_ERROR_NONE);
      }
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnRenameEvent(
    ash::disks::DiskMountManager::RenameEvent event,
    chromeos::RenameError error_code,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnDeviceEvent: " << event << ", " << error_code << ", "
           << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::RENAME_STARTED:
      for (auto& observer : observers_) {
        observer.OnRenameStarted(device_path, device_label,
                                 error_code == chromeos::RENAME_ERROR_NONE);
      }
      return;
    case ash::disks::DiskMountManager::RENAME_COMPLETED:
      // Find previous mount point label if it exists
      std::string mount_label;
      auto disk_map_iter = disk_mount_manager_->disks().find(device_path);
      if (disk_map_iter != disk_mount_manager_->disks().end() &&
          !disk_map_iter->second->base_mount_path().empty()) {
        mount_label = base::FilePath(disk_map_iter->second->base_mount_path())
                          .BaseName()
                          .AsUTF8Unsafe();
      }

      // Try to mount the device. MountPath auto-detects filesystem format if
      // second argument is empty. Third argument is a mount point name of the
      // disk when it was first time mounted (to preserve mount point regardless
      // of the volume name).
      disk_mount_manager_->MountPath(device_path, std::string(), mount_label,
                                     {}, chromeos::MOUNT_TYPE_DEVICE,
                                     GetExternalStorageAccessMode(profile_),
                                     base::DoNothing());

      bool successfully_renamed = error_code == chromeos::RENAME_ERROR_NONE;
      for (auto& observer : observers_)
        observer.OnRenameCompleted(device_path, device_label,
                                   successfully_renamed);

      return;
  }
  NOTREACHED();
}

void VolumeManager::OnProvidedFileSystemMount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    ash::file_system_provider::MountContext context,
    base::File::Error error) {
  MountContext volume_context = MOUNT_CONTEXT_UNKNOWN;
  switch (context) {
    case ash::file_system_provider::MOUNT_CONTEXT_USER:
      volume_context = MOUNT_CONTEXT_USER;
      break;
    case ash::file_system_provider::MOUNT_CONTEXT_RESTORE:
      volume_context = MOUNT_CONTEXT_AUTO;
      break;
  }

  std::unique_ptr<Volume> volume =
      Volume::CreateForProvidedFileSystem(file_system_info, volume_context);

  // TODO(mtomasz): Introduce own type, and avoid using MountError internally,
  // since it is related to cros disks only.
  chromeos::MountError mount_error;
  switch (error) {
    case base::File::FILE_OK:
      mount_error = chromeos::MOUNT_ERROR_NONE;
      break;
    case base::File::FILE_ERROR_EXISTS:
      mount_error = chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED;
      break;
    default:
      mount_error = chromeos::MOUNT_ERROR_UNKNOWN;
      break;
  }

  DoMountEvent(mount_error, std::move(volume));
}

void VolumeManager::OnProvidedFileSystemUnmount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // TODO(mtomasz): Introduce own type, and avoid using MountError internally,
  // since it is related to cros disks only.
  const chromeos::MountError mount_error = error == base::File::FILE_OK
                                               ? chromeos::MOUNT_ERROR_NONE
                                               : chromeos::MOUNT_ERROR_UNKNOWN;
  std::unique_ptr<Volume> volume = Volume::CreateForProvidedFileSystem(
      file_system_info, MOUNT_CONTEXT_UNKNOWN);
  DoUnmountEvent(mount_error, *volume);
}

void VolumeManager::OnExternalStorageDisabledChangedUnmountCallback(
    std::vector<std::string> remaining_mount_paths,
    chromeos::MountError error_code) {
  LOG_IF(ERROR, error_code != chromeos::MOUNT_ERROR_NONE)
      << "Unmount on ExternalStorageDisabled policy change failed: "
      << error_code;

  while (!remaining_mount_paths.empty()) {
    std::string mount_path = remaining_mount_paths.back();
    remaining_mount_paths.pop_back();
    if (!base::Contains(disk_mount_manager_->mount_points(), mount_path)) {
      // The mount point could have already been removed for another reason
      // (i.e. the disk was removed by the user).
      continue;
    }

    disk_mount_manager_->UnmountPath(
        mount_path,
        base::BindOnce(
            &VolumeManager::OnExternalStorageDisabledChangedUnmountCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(remaining_mount_paths)));
    return;
  }
}

void VolumeManager::OnArcPlayStoreEnabledChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(arc::kMediaViewFeature));
  DCHECK(arc::IsArcAllowedForProfile(profile_));

  if (enabled == arc_volumes_mounted_)
    return;

  if (enabled) {
    DoMountEvent(chromeos::MOUNT_ERROR_NONE,
                 Volume::CreateForMediaView(arc::kImagesRootDocumentId));
    DoMountEvent(chromeos::MOUNT_ERROR_NONE,
                 Volume::CreateForMediaView(arc::kVideosRootDocumentId));
    DoMountEvent(chromeos::MOUNT_ERROR_NONE,
                 Volume::CreateForMediaView(arc::kAudioRootDocumentId));
    DoMountEvent(
        chromeos::MOUNT_ERROR_NONE,
        Volume::CreateForAndroidFiles(base::FilePath(util::kAndroidFilesPath)));
  } else {
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForMediaView(arc::kImagesRootDocumentId));
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForMediaView(arc::kVideosRootDocumentId));
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForMediaView(arc::kAudioRootDocumentId));
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   *Volume::CreateForAndroidFiles(
                       base::FilePath(util::kAndroidFilesPath)));
  }

  documents_provider_root_manager_->SetEnabled(enabled);
  arc_volumes_mounted_ = enabled;
}

void VolumeManager::OnExternalStorageDisabledChanged() {
  // If the policy just got disabled we have to unmount every device currently
  // mounted. The opposite is fine - we can let the user re-plug their device to
  // make it available.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    // We do not iterate on mount_points directly, because mount_points can
    // be changed by UnmountPath(). Also, a failing unmount shouldn't be retried
    // indefinitely. So make a set of all the mount points that should be
    // unmounted (all external media mounts), and iterate through them.
    std::vector<std::string> remaining_mount_paths;
    for (auto& mount_point : disk_mount_manager_->mount_points()) {
      if (mount_point.second.mount_type == chromeos::MOUNT_TYPE_DEVICE) {
        remaining_mount_paths.push_back(mount_point.first);
      }
    }
    if (remaining_mount_paths.empty()) {
      return;
    }

    std::string mount_path = remaining_mount_paths.back();
    remaining_mount_paths.pop_back();
    disk_mount_manager_->UnmountPath(
        mount_path,
        base::BindOnce(
            &VolumeManager::OnExternalStorageDisabledChangedUnmountCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(remaining_mount_paths)));
  }
}

void VolumeManager::OnExternalStorageReadOnlyChanged() {
  disk_mount_manager_->RemountAllRemovableDrives(
      GetExternalStorageAccessMode(profile_));
}

void VolumeManager::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id()))
    return;
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled))
    return;

  // Resolve mtp storage name and get MtpStorageInfo.
  std::string storage_name;
  base::RemoveChars(info.location(), kRootPath, &storage_name);
  DCHECK(!storage_name.empty());
  if (get_mtp_storage_info_callback_.is_null()) {
    storage_monitor::StorageMonitor::GetInstance()
        ->media_transfer_protocol_manager()
        ->GetStorageInfo(storage_name,
                         base::BindOnce(&VolumeManager::DoAttachMtpStorage,
                                        weak_ptr_factory_.GetWeakPtr(), info));
  } else {
    get_mtp_storage_info_callback_.Run(
        storage_name, base::BindOnce(&VolumeManager::DoAttachMtpStorage,
                                     weak_ptr_factory_.GetWeakPtr(), info));
  }
}

void VolumeManager::DoAttachMtpStorage(
    const storage_monitor::StorageInfo& info,
    device::mojom::MtpStorageInfoPtr mtp_storage_info) {
  if (!mtp_storage_info) {
    // |mtp_storage_info| can be null. e.g. As OnRemovableStorageAttached and
    // DoAttachMtpStorage are called asynchronously, there can be a race
    // condition where the storage has been already removed in
    // MediaTransferProtocolManager at the time when this method is called.
    return;
  }

  // Mtp write is enabled only when the device is writable, supports generic
  // hierarchical file system, and writing to external storage devices is not
  // prohibited by the preference.
  const bool read_only =
      mtp_storage_info->access_capability != kAccessCapabilityReadWrite ||
      mtp_storage_info->filesystem_type !=
          kFilesystemTypeGenericHierarchical ||
      GetExternalStorageAccessMode(profile_) ==
          chromeos::MOUNT_ACCESS_MODE_READ_ONLY;

  const base::FilePath path = base::FilePath::FromUTF8Unsafe(info.location());
  const std::string fsid = GetMountPointNameForMediaStorage(info);
  const std::string base_name = base::UTF16ToUTF8(info.model_name());

  // Assign a fresh volume ID based on the volume name.
  std::string label = base_name;
  for (int i = 2; mounted_volumes_.count(kMtpVolumeIdPrefix + label); ++i)
    label = base_name + base::StringPrintf(" (%d)", i);

  bool result =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          fsid, storage::kFileSystemTypeDeviceMediaAsFileStorage,
          storage::FileSystemMountOption(), path);
  DCHECK(result);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MTPDeviceMapService::RegisterMTPFileSystem,
                     base::Unretained(MTPDeviceMapService::GetInstance()),
                     info.location(), fsid, read_only));

  std::unique_ptr<Volume> volume = Volume::CreateForMTP(path, label, read_only);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, std::move(volume));
}

void VolumeManager::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id()))
    return;

  for (const auto& mounted_volume : mounted_volumes_) {
    if (mounted_volume.second->source_path().value() == info.location()) {
      DoUnmountEvent(chromeos::MOUNT_ERROR_NONE, *mounted_volume.second.get());

      const std::string fsid = GetMountPointNameForMediaStorage(info);
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(fsid);
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MTPDeviceMapService::RevokeMTPFileSystem,
                         base::Unretained(MTPDeviceMapService::GetInstance()),
                         fsid));
      return;
    }
  }
}

void VolumeManager::OnDocumentsProviderRootAdded(
    const std::string& authority,
    const std::string& root_id,
    const std::string& document_id,
    const std::string& title,
    const std::string& summary,
    const GURL& icon_url,
    bool read_only,
    const std::vector<std::string>& mime_types) {
  arc::ArcDocumentsProviderRootMap::GetForArcBrowserContext()->RegisterRoot(
      authority, document_id, root_id, read_only, mime_types);
  DoMountEvent(
      chromeos::MOUNT_ERROR_NONE,
      Volume::CreateForDocumentsProvider(authority, root_id, document_id, title,
                                         summary, icon_url, read_only));
}

void VolumeManager::OnDocumentsProviderRootRemoved(
    const std::string& authority,
    const std::string& root_id,
    const std::string& document_id) {
  DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                 *Volume::CreateForDocumentsProvider(
                     authority, root_id, std::string(), std::string(),
                     std::string(), GURL(), false));
  arc::ArcDocumentsProviderRootMap::GetForArcBrowserContext()->UnregisterRoot(
      authority, document_id);
}

void VolumeManager::AddSmbFsVolume(const base::FilePath& mount_point,
                                   const std::string& display_name) {
  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               Volume::CreateForSmb(mount_point, display_name));
}

void VolumeManager::RemoveSmbFsVolume(const base::FilePath& mount_point) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                 *Volume::CreateForSmb(mount_point, ""));
}

void VolumeManager::OnDiskMountManagerRefreshed(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to refresh disk mount manager";
    return;
  }

  std::vector<std::unique_ptr<Volume>> archives;

  const ash::disks::DiskMountManager::MountPointMap& mount_points =
      disk_mount_manager_->mount_points();
  for (const auto& mount_point : mount_points) {
    switch (mount_point.second.mount_type) {
      case chromeos::MOUNT_TYPE_ARCHIVE: {
        // Archives are mounted after other types of volume. See below.
        archives.push_back(
            Volume::CreateForRemovable(mount_point.second, nullptr));
        break;
      }
      case chromeos::MOUNT_TYPE_DEVICE: {
        DoMountEvent(
            chromeos::MOUNT_ERROR_NONE,
            Volume::CreateForRemovable(
                mount_point.second, disk_mount_manager_->FindDiskBySourcePath(
                                        mount_point.second.source_path)));
        break;
      }
      case chromeos::MOUNT_TYPE_NETWORK_STORAGE: {
        break;
      }
      case chromeos::MOUNT_TYPE_INVALID: {
        NOTREACHED();
      }
    }
  }

  // We mount archives only if they are opened from currently mounted volumes.
  // To check the condition correctly in DoMountEvent, we care about the order.
  std::vector<bool> done(archives.size(), false);
  for (size_t i = 0; i < archives.size(); ++i) {
    if (done[i])
      continue;

    std::vector<std::unique_ptr<Volume>> chain;
    // done[x] = true means archives[x] is null and that volume is in |chain|.
    done[i] = true;
    chain.push_back(std::move(archives[i]));

    // If archives[i]'s source_path is in another archive, mount it first.
    for (size_t parent = i + 1; parent < archives.size(); ++parent) {
      if (!done[parent] &&
          archives[parent]->mount_path().IsParent(
              chain.back()->source_path())) {
        // done[parent] started false, so archives[parent] is non-null.
        done[parent] = true;
        chain.push_back(std::move(archives[parent]));
        parent = i + 1;  // Search archives[parent]'s parent from the beginning.
      }
    }

    // Mount from the tail of chain.
    for (size_t i = chain.size(); i > 0; --i) {
      DoMountEvent(chromeos::MOUNT_ERROR_NONE, std::move(chain[i - 1]));
    }
  }
}

void VolumeManager::OnStorageMonitorInitialized() {
  std::vector<storage_monitor::StorageInfo> storages =
      storage_monitor::StorageMonitor::GetInstance()->GetAllAvailableStorages();
  for (size_t i = 0; i < storages.size(); ++i)
    OnRemovableStorageAttached(storages[i]);
  storage_monitor::StorageMonitor::GetInstance()->AddObserver(this);
}

void VolumeManager::DoMountEvent(chromeos::MountError error_code,
                                 std::unique_ptr<Volume> volume) {
  // Archive files are mounted globally in system. We however don't want to show
  // archives from profile-specific folders (Drive/Downloads) of other users in
  // multi-profile session. To this end, we filter out archives not on the
  // volumes already mounted on this VolumeManager instance.
  if (volume->type() == VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
    // Source may be in Drive cache folder under the current profile directory.
    bool from_current_profile =
        profile_->GetPath().IsParent(volume->source_path());
    for (const auto& mounted_volume : mounted_volumes_) {
      if (mounted_volume.second->mount_path().IsParent(volume->source_path())) {
        from_current_profile = true;
        break;
      }
    }
    if (!from_current_profile)
      return;
  }

  // Filter out removable disks if forbidden by policy for this profile.
  if (volume->type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION &&
      profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    return;
  }

  Volume* raw_volume = volume.get();
  if (error_code == chromeos::MOUNT_ERROR_NONE || volume->mount_condition()) {
    mounted_volumes_[volume->volume_id()] = std::move(volume);
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.VolumeType", raw_volume->type(),
                              NUM_VOLUME_TYPE);
  }

  for (auto& observer : observers_)
    observer.OnVolumeMounted(error_code, *raw_volume);
}

void VolumeManager::DoUnmountEvent(chromeos::MountError error_code,
                                   const Volume& volume) {
  auto iter = mounted_volumes_.find(volume.volume_id());
  if (iter == mounted_volumes_.end())
    return;
  std::unique_ptr<Volume> volume_ref;
  if (error_code == chromeos::MOUNT_ERROR_NONE) {
    // It is important to hold a reference to the removed Volume from
    // |mounted_volumes_|, because OnVolumeMounted() will access it.
    volume_ref = std::move(iter->second);
    mounted_volumes_.erase(iter);
  }

  for (auto& observer : observers_)
    observer.OnVolumeUnmounted(error_code, volume);
}

base::FilePath VolumeManager::GetDriveMountPointPath() const {
  return drive_integration_service_->GetMountPointPath();
}

void VolumeManager::OnSshfsCrostiniUnmountCallback(
    const base::FilePath& sshfs_mount_path,
    RemoveSshfsCrostiniVolumeCallback callback,
    chromeos::MountError error_code) {
  if ((error_code == chromeos::MOUNT_ERROR_NONE) ||
      (error_code == chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED)) {
    // Remove metadata associated with the mount. It will be a no-op if it
    // wasn't mounted or unmounted out of band.
    DoUnmountEvent(
        chromeos::MOUNT_ERROR_NONE,
        *Volume::CreateForSshfsCrostini(sshfs_mount_path, base::FilePath()));
    if (callback)
      std::move(callback).Run(true);
    return;
  }

  LOG(ERROR) << "Unmounting " << sshfs_mount_path.value() << " failed";
  if (callback)
    std::move(callback).Run(false);
}

}  // namespace file_manager
