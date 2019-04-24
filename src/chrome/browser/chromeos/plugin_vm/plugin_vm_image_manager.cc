// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/zlib/google/zip.h"

namespace plugin_vm {

bool PluginVmImageManager::IsProcessingImage() {
  return processing_image_;
}

void PluginVmImageManager::StartDownload() {
  if (IsProcessingImage()) {
    LOG(ERROR) << "Download of a PluginVm image couldn't be started as"
               << " another PluginVm image is currently being processed";
    OnDownloadFailed();
    return;
  }
  processing_image_ = true;
  GURL url = GetPluginVmImageDownloadUrl();
  if (url.is_empty()) {
    OnDownloadFailed();
    return;
  }
  download_service_->StartDownload(GetDownloadParams(url));
}

void PluginVmImageManager::CancelDownload() {
  download_service_->CancelDownload(current_download_guid_);
}

void PluginVmImageManager::StartUnzipping() {
  if (IsDownloading()) {
    LOG(ERROR) << "Unzipping of PluginVm image couldn't be proceeded "
               << "as image is still being downloaded";
    OnUnzipped(false);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PluginVmImageManager::UnzipDownloadedPluginVmImageArchive,
                     base::Unretained(this)),
      base::BindOnce(&PluginVmImageManager::OnUnzipped,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmImageManager::CancelUnzipping() {
  unzipping_cancelled_ = true;
}

void PluginVmImageManager::SetObserver(Observer* observer) {
  observer_ = observer;
}

void PluginVmImageManager::RemoveObserver() {
  observer_ = nullptr;
}

void PluginVmImageManager::OnDownloadStarted() {
  if (observer_)
    observer_->OnDownloadStarted();
}

void PluginVmImageManager::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                     int64_t content_length) {
  if (observer_)
    observer_->OnDownloadProgressUpdated(bytes_downloaded, content_length);
}

void PluginVmImageManager::OnDownloadCompleted(
    const download::CompletionInfo& info) {
  downloaded_plugin_vm_image_archive_ = info.path;
  current_download_guid_.clear();

  if (!VerifyDownload(info.hash256)) {
    LOG(ERROR) << "Downloaded PluginVm image archive hash doesn't match "
               << "hash specified by the PluginVmImage policy";
    OnDownloadFailed();
    return;
  }

  if (observer_)
    observer_->OnDownloadCompleted();
}

void PluginVmImageManager::OnDownloadCancelled() {
  RemoveTemporaryPluginVmImageArchiveIfExists();
  current_download_guid_.clear();
  processing_image_ = false;
  if (observer_)
    observer_->OnDownloadCancelled();
}

void PluginVmImageManager::OnDownloadFailed() {
  RemoveTemporaryPluginVmImageArchiveIfExists();
  current_download_guid_.clear();
  processing_image_ = false;
  if (observer_)
    observer_->OnDownloadFailed();
}

void PluginVmImageManager::OnUnzippingProgressUpdated(int new_unzipped_bytes) {
  plugin_vm_image_bytes_unzipped_ += new_unzipped_bytes;
  if (observer_)
    observer_->OnUnzippingProgressUpdated(plugin_vm_image_bytes_unzipped_,
                                          plugin_vm_image_size_);
}

void PluginVmImageManager::OnUnzipped(bool success) {
  unzipping_cancelled_ = false;
  plugin_vm_image_size_ = -1;
  plugin_vm_image_bytes_unzipped_ = 0;
  RemoveTemporaryPluginVmImageArchiveIfExists();
  if (!success) {
    if (observer_)
      observer_->OnUnzippingFailed();
    RemovePluginVmImageDirectoryIfExists();
    processing_image_ = false;
    return;
  }
  if (observer_)
    observer_->OnUnzipped();
  processing_image_ = false;
  // TODO(okalitova): Populate necessary preferences with information about
  // PluginVm image that has been successfully downloaded and extracted.
}

void PluginVmImageManager::SetDownloadServiceForTesting(
    download::DownloadService* download_service) {
  download_service_ = download_service;
}

void PluginVmImageManager::SetDownloadedPluginVmImageArchiveForTesting(
    const base::FilePath& downloaded_plugin_vm_image_archive) {
  downloaded_plugin_vm_image_archive_ = downloaded_plugin_vm_image_archive;
}

std::string PluginVmImageManager::GetCurrentDownloadGuidForTesting() {
  return current_download_guid_;
}

PluginVmImageManager::PluginVmImageManager(Profile* profile)
    : profile_(profile),
      download_service_(DownloadServiceFactory::GetForBrowserContext(profile)),
      weak_ptr_factory_(this) {}
PluginVmImageManager::~PluginVmImageManager() = default;

GURL PluginVmImageManager::GetPluginVmImageDownloadUrl() {
  const base::Value* url_ptr =
      profile_->GetPrefs()
          ->GetDictionary(plugin_vm::prefs::kPluginVmImage)
          ->FindKey("url");
  if (!url_ptr) {
    LOG(ERROR) << "Url to PluginVm image is not specified";
    return GURL();
  }
  return GURL(url_ptr->GetString());
}

download::DownloadParams PluginVmImageManager::GetDownloadParams(
    const GURL& url) {
  download::DownloadParams params;

  // DownloadParams
  params.client = download::DownloadClient::PLUGIN_VM_IMAGE;
  params.guid = base::GenerateGUID();
  params.callback = base::BindRepeating(&PluginVmImageManager::OnStartDownload,
                                        weak_ptr_factory_.GetWeakPtr());
  // TODO(okalitova): Create annotation.
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(NO_TRAFFIC_ANNOTATION_YET);

  // RequestParams

  params.request_params.url = url;
  params.request_params.method = "GET";

  // SchedulingParams
  // User initiates download by clicking on PluginVm icon so priorities should
  // be the highest.
  params.scheduling_params.priority = download::SchedulingParams::Priority::UI;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;

  return params;
}

void PluginVmImageManager::OnStartDownload(
    const std::string& download_guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::ACCEPTED)
    current_download_guid_ = download_guid;
  else
    OnDownloadFailed();
}

bool PluginVmImageManager::IsDownloading() {
  return !current_download_guid_.empty();
}

bool PluginVmImageManager::VerifyDownload(
    const std::string& downloaded_archive_hash) {
  // Hash should be there in the common case. However, there are situations,
  // when hash could not be available, for example, when download is parallel
  // or the completion of download is reported after restart. Therefore, hash
  // not being specified should not resolve in download being considered
  // unsuccessful, but should be logged.
  // TODO(okalitova): Consider download as unsuccessful once hash should always
  // be in place.
  if (downloaded_archive_hash.empty()) {
    LOG(WARNING) << "No hash for downloaded PluginVm image archive";
    return true;
  }
  const base::Value* plugin_vm_image_hash_ptr =
      profile_->GetPrefs()
          ->GetDictionary(plugin_vm::prefs::kPluginVmImage)
          ->FindKey("hash");
  if (!plugin_vm_image_hash_ptr) {
    LOG(ERROR) << "Hash of PluginVm image is not specified";
    return false;
  }
  std::string plugin_vm_image_hash = plugin_vm_image_hash_ptr->GetString();

  return base::EqualsCaseInsensitiveASCII(plugin_vm_image_hash,
                                          downloaded_archive_hash);
}

void PluginVmImageManager::CalculatePluginVmImageSize() {
  plugin_vm_image_size_ = 0;

  zip::ZipReader reader;
  if (!reader.Open(downloaded_plugin_vm_image_archive_)) {
    LOG(ERROR) << downloaded_plugin_vm_image_archive_.value()
               << " cannot be opened by ZipReader";
    plugin_vm_image_size_ = -1;
    return;
  }

  while (reader.HasMore()) {
    if (!reader.OpenCurrentEntryInZip()) {
      LOG(ERROR) << "One of zip entries cannot be opened";
      plugin_vm_image_size_ = -1;
      return;
    }
    plugin_vm_image_size_ += reader.current_entry_info()->original_size();
    if (!reader.AdvanceToNextEntry()) {
      LOG(ERROR) << "ZipReader failed to advance to the next entry";
      plugin_vm_image_size_ = -1;
      return;
    }
  }
}

bool PluginVmImageManager::UnzipDownloadedPluginVmImageArchive() {
  if (!EnsureDirectoryForPluginVmImageIsPresent() ||
      !EnsureDownloadedPluginVmImageArchiveIsPresent()) {
    LOG(ERROR) << "Unzipping of PluginVm image couldn't be proceeded";
    return false;
  }

  CalculatePluginVmImageSize();

  base::File file(downloaded_plugin_vm_image_archive_,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open "
               << downloaded_plugin_vm_image_archive_.value();
    return false;
  }

  plugin_vm_image_bytes_unzipped_ = 0;
  bool success = zip::UnzipWithFilterAndWriters(
      file.GetPlatformFile(),
      base::BindRepeating(
          &PluginVmImageManager::CreatePluginVmImageWriterDelegate,
          base::Unretained(this)),
      base::BindRepeating(&PluginVmImageManager::CreateDirectory,
                          base::Unretained(this)),
      base::BindRepeating(
          &PluginVmImageManager::FilterFilesInPluginVmImageArchive,
          base::Unretained(this)),
      true /* log_skipped_files */);
  return success;
}

bool PluginVmImageManager::IsUnzippingCancelled() {
  return unzipping_cancelled_;
}

PluginVmImageManager::PluginVmImageWriterDelegate::PluginVmImageWriterDelegate(
    PluginVmImageManager* manager,
    const base::FilePath& output_file_path)
    : manager_(manager), output_file_path_(output_file_path) {}

bool PluginVmImageManager::PluginVmImageWriterDelegate::PrepareOutput() {
  // We can't rely on parent directory entries being specified in the
  // zip, so we make sure they are created.
  if (!base::CreateDirectory(output_file_path_.DirName()))
    return false;

  output_file_.Initialize(output_file_path_, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);
  return output_file_.IsValid();
}

bool PluginVmImageManager::PluginVmImageWriterDelegate::WriteBytes(
    const char* data,
    int num_bytes) {
  bool success = num_bytes == output_file_.WriteAtCurrentPos(data, num_bytes);
  if (success) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&PluginVmImageManager::OnUnzippingProgressUpdated,
                       base::Unretained(manager_), num_bytes));
  }
  return !manager_->IsUnzippingCancelled() && success;
}

void PluginVmImageManager::PluginVmImageWriterDelegate::SetTimeModified(
    const base::Time& time) {
  output_file_.Close();
  base::TouchFile(output_file_path_, base::Time::Now(), time);
}

std::unique_ptr<zip::WriterDelegate>
PluginVmImageManager::CreatePluginVmImageWriterDelegate(
    const base::FilePath& entry_path) {
  return std::make_unique<PluginVmImageWriterDelegate>(
      this, plugin_vm_image_dir_.Append(entry_path));
}

bool PluginVmImageManager::CreateDirectory(const base::FilePath& entry_path) {
  return base::CreateDirectory(plugin_vm_image_dir_.Append(entry_path));
}

bool PluginVmImageManager::FilterFilesInPluginVmImageArchive(
    const base::FilePath& file) {
  return true;
}

bool PluginVmImageManager::EnsureDownloadedPluginVmImageArchiveIsPresent() {
  return !downloaded_plugin_vm_image_archive_.empty();
}

bool PluginVmImageManager::EnsureDirectoryForPluginVmImageIsPresent() {
  plugin_vm_image_dir_ = profile_->GetPath()
                             .AppendASCII(kCrosvmDir)
                             .AppendASCII(kPvmDir)
                             .AppendASCII(kPluginVmImageDir);
  if (!base::CreateDirectory(plugin_vm_image_dir_)) {
    LOG(ERROR) << "Directory " << plugin_vm_image_dir_.value()
               << " failed to be created";
    plugin_vm_image_dir_.clear();
    return false;
  }
  return true;
}

void PluginVmImageManager::RemoveTemporaryPluginVmImageArchiveIfExists() {
  if (!downloaded_plugin_vm_image_archive_.empty()) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&base::DeleteFile, downloaded_plugin_vm_image_archive_,
                       false /* recursive */),
        base::BindOnce(
            &PluginVmImageManager::OnTemporaryPluginVmImageArchiveRemoved,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void PluginVmImageManager::OnTemporaryPluginVmImageArchiveRemoved(
    bool success) {
  if (!success) {
    LOG(ERROR) << "Downloaded PluginVm image archive located in "
               << downloaded_plugin_vm_image_archive_.value()
               << " failed to be deleted";
    return;
  }
  downloaded_plugin_vm_image_archive_.clear();
}

void PluginVmImageManager::RemovePluginVmImageDirectoryIfExists() {
  if (!plugin_vm_image_dir_.empty()) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&base::DeleteFile, plugin_vm_image_dir_,
                       true /* recursive */),
        base::BindOnce(&PluginVmImageManager::OnPluginVmImageDirectoryRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PluginVmImageManager::OnPluginVmImageDirectoryRemoved(bool success) {
  if (!success) {
    LOG(ERROR) << "Directory with PluginVm image "
               << plugin_vm_image_dir_.value() << " failed to be deleted";
    return;
  }
  plugin_vm_image_dir_.clear();
}

}  // namespace plugin_vm
