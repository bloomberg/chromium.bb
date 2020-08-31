// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_launch/web_launch_files_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"
#include "url/origin.h"

namespace web_launch {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLaunchFilesHelper)

// static
void WebLaunchFilesHelper::SetLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths) {
  if (launch_paths.size() == 0)
    return;

  web_contents->SetUserData(
      UserDataKey(), std::make_unique<WebLaunchFilesHelper>(
                         web_contents, launch_url, std::move(launch_paths)));
}

// static
void WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths) {
  if (launch_dir.empty())
    return;

  if (launch_paths.size() == 0)
    return;

  web_contents->SetUserData(UserDataKey(),
                            std::make_unique<WebLaunchFilesHelper>(
                                web_contents, launch_url, std::move(launch_dir),
                                std::move(launch_paths)));
}

void WebLaunchFilesHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Currently, launch data is only sent for the main frame.
  if (!handle->IsInMainFrame())
    return;

  MaybeSendLaunchEntries();
}

namespace {

class EntriesBuilder {
 public:
  EntriesBuilder(
      std::vector<blink::mojom::NativeFileSystemEntryPtr>* entries_ref,
      content::WebContents* web_contents,
      const GURL& launch_url)
      : entries_ref_(entries_ref),
        entry_factory_(web_contents->GetMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetNativeFileSystemEntryFactory()),
        context_(url::Origin::Create(launch_url),
                 launch_url,
                 web_contents->GetMainFrame()->GetProcess()->GetID(),
                 web_contents->GetMainFrame()->GetRoutingID()) {}

  void AddFileEntry(const base::FilePath& path) {
    entries_ref_->push_back(
        entry_factory_->CreateFileEntryFromPath(context_, path));
  }
  void AddDirectoryEntry(const base::FilePath& path) {
    entries_ref_->push_back(
        entry_factory_->CreateDirectoryEntryFromPath(context_, path));
  }

 private:
  std::vector<blink::mojom::NativeFileSystemEntryPtr>* entries_ref_;
  scoped_refptr<content::NativeFileSystemEntryFactory> entry_factory_;
  content::NativeFileSystemEntryFactory::BindingContext context_;
};

}  // namespace

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents), launch_url_(launch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(launch_paths.size());

  launch_entries_.reserve(launch_paths.size());

  EntriesBuilder entries_builder(&launch_entries_, web_contents, launch_url);
  for (const auto& path : launch_paths)
    entries_builder.AddFileEntry(path);

  // Asynchronously call MaybeSendLaunchEntries, since it may destroy |this|.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&WebLaunchFilesHelper::MaybeSendLaunchEntries,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents), launch_url_(launch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!launch_dir.empty());
  DCHECK(launch_paths.size());

  launch_entries_.reserve(launch_paths.size() + 1);

  EntriesBuilder entries_builder(&launch_entries_, web_contents, launch_url);
  entries_builder.AddDirectoryEntry(launch_dir);
  for (const auto& path : launch_paths)
    entries_builder.AddFileEntry(path);

  // Asynchronously call MaybeSendLaunchEntries, since it may destroy |this|.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&WebLaunchFilesHelper::MaybeSendLaunchEntries,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebLaunchFilesHelper::~WebLaunchFilesHelper() = default;

void WebLaunchFilesHelper::MaybeSendLaunchEntries() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (launch_entries_.size() == 0)
    return;
  if (launch_url_ != web_contents()->GetLastCommittedURL())
    return;

  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &launch_service);
  DCHECK(launch_service);
  launch_service->SetLaunchFiles(std::move(launch_entries_));

  // LaunchParams are sent, clean up.
  web_contents()->RemoveUserData(UserDataKey());
}

}  // namespace web_launch
