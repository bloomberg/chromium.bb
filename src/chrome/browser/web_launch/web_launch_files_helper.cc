// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_launch/web_launch_files_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/origin.h"

namespace web_launch {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLaunchFilesHelper);

namespace {

// On Chrome OS paths that exist on an external mount point need to be treated
// differently to make sure the File System Access code accesses these paths via
// the correct file system backend. This method checks if this is the case, and
// updates `entry_path` to the path that should be used by the File System
// Access implementation.
content::FileSystemAccessEntryFactory::PathType MaybeRemapPath(
    base::FilePath* entry_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath virtual_path;
  auto* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (external_mount_points->GetVirtualPath(*entry_path, &virtual_path)) {
    *entry_path = std::move(virtual_path);
    return content::FileSystemAccessEntryFactory::PathType::kExternal;
  }
#endif
  return content::FileSystemAccessEntryFactory::PathType::kLocal;
}

class EntriesBuilder {
 public:
  EntriesBuilder(content::WebContents* web_contents,
                 const GURL& launch_url,
                 size_t expected_number_of_entries)
      : entry_factory_(web_contents->GetMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetFileSystemAccessEntryFactory()),
        context_(blink::StorageKey(url::Origin::Create(launch_url)),
                 launch_url,
                 content::GlobalRenderFrameHostId(
                     web_contents->GetMainFrame()->GetProcess()->GetID(),
                     web_contents->GetMainFrame()->GetRoutingID())) {
    entries_.reserve(expected_number_of_entries);
  }

  void AddFileEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateFileEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  void AddDirectoryEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateDirectoryEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> Build() {
    return std::move(entries_);
  }

 private:
  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries_;
  scoped_refptr<content::FileSystemAccessEntryFactory> entry_factory_;
  content::FileSystemAccessEntryFactory::BindingContext context_;
};

}  // namespace

WebLaunchFilesHelper::~WebLaunchFilesHelper() = default;

// static
WebLaunchFilesHelper* WebLaunchFilesHelper::GetForWebContents(
    content::WebContents* web_contents) {
  return static_cast<WebLaunchFilesHelper*>(
      web_contents->GetUserData(UserDataKey()));
}

// static
void WebLaunchFilesHelper::EnqueueLaunchParams(
    content::WebContents* web_contents,
    const GURL& app_scope,
    bool await_navigation,
    GURL launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths) {
  auto helper = base::WrapUnique(
      new WebLaunchFilesHelper(web_contents, app_scope, std::move(launch_url),
                               std::move(launch_dir), std::move(launch_paths)));

  auto* helper_ptr = helper.get();
  web_contents->SetUserData(UserDataKey(), std::move(helper));
  helper_ptr->Start(await_navigation);
}

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& app_scope,
    GURL launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebLaunchFilesHelper>(*web_contents),
      app_scope_(app_scope.spec()),
      launch_url_(std::move(launch_url)),
      launch_dir_(std::move(launch_dir)),
      launch_paths_(std::move(launch_paths)) {
  DCHECK(InAppScope(launch_url_));
}

void WebLaunchFilesHelper::Start(bool await_navigation) {
  // Wait for DidFinishNavigation before enqueuing.
  if (await_navigation)
    return;

  url_params_enqueued_in_ = web_contents()->GetLastCommittedURL();
  SendLaunchEntries();
}

// TODO(crbug.com/1250225): Move this class into chrome/browser/web_applications
// and use WebAppRegistrar::IsUrlInAppScope().
bool WebLaunchFilesHelper::InAppScope(const GURL& url) const {
  return base::StartsWith(url.spec(), app_scope_, base::CompareCase::SENSITIVE);
}

void WebLaunchFilesHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  // Currently, launch data is only sent for the main frame.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!handle->IsInPrimaryMainFrame())
    return;

  // Launch params still haven't been enqueued.
  if (!url_params_enqueued_in_.is_valid()) {
    if (!InAppScope(handle->GetURL())) {
      DestroySelf();
      return;
    }

    url_params_enqueued_in_ = handle->GetURL();
    SendLaunchEntries();
    return;
  }

  // Re-enqueue launch params for page reloads.
  // Check the current URL still matches as it may have changed via
  // `history.pushState()`.
  if (handle->GetReloadType() != content::ReloadType::NONE &&
      url_params_enqueued_in_ == handle->GetURL()) {
    SendLaunchEntries();
    return;
  }

  DestroySelf();
  return;
}

void WebLaunchFilesHelper::SendLaunchEntries() {
  DCHECK(url_params_enqueued_in_.is_valid());
  DCHECK(InAppScope(url_params_enqueued_in_));
  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &launch_service);
  DCHECK(launch_service);

  if (!launch_paths_.empty() || !launch_dir_.empty()) {
    EntriesBuilder entries_builder(web_contents(), launch_url_,
                                   launch_paths_.size() + 1);
    if (!launch_dir_.empty())
      entries_builder.AddDirectoryEntry(launch_dir_);

    for (const auto& path : launch_paths_)
      entries_builder.AddFileEntry(path);

    launch_service->SetLaunchFiles(entries_builder.Build());
  } else {
    launch_service->EnqueueLaunchParams(launch_url_);
  }
}

void WebLaunchFilesHelper::CloseApp() {
  web_contents()->Close();
  // `this` is deleted.
}

void WebLaunchFilesHelper::DestroySelf() {
  web_contents()->RemoveUserData(UserDataKey());
  // `this` is deleted.
}

}  // namespace web_launch
