// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_MAC_SHARING_SERVICE_OPERATION_H_
#define CHROME_BROWSER_WEBSHARE_MAC_SHARING_SERVICE_OPERATION_H_

#include <string>
#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace webshare {

class PrepareDirectoryTask;
class PrepareSubDirectoryTask;

// macOS implementation of navigator.share()
// This class invokes mac native NSSharingServicePicker from a wrapper class in
// remote_cocoa after validation of data and in case of files first kicks off
// tasks to create a directories and storing files in them.
class SharingServiceOperation final : content::WebContentsObserver {
 public:
  using SharePickerCallback = base::RepeatingCallback<void(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      blink::mojom::ShareService::ShareCallback close_callback)>;

  SharingServiceOperation(const std::string& title,
                          const std::string& text,
                          const GURL& url,
                          std::vector<blink::mojom::SharedFilePtr> files,
                          content::WebContents* web_contents);
  ~SharingServiceOperation() override;
  SharingServiceOperation(const SharingServiceOperation&) = delete;
  SharingServiceOperation& operator=(const SharingServiceOperation&) = delete;

  void Share(blink::mojom::ShareService::ShareCallback callback);

  static void SetSharePickerCallbackForTesting(SharePickerCallback callback);

 private:
  void OnPrepareDirectory(blink::mojom::ShareError error);
  void OnPrepareSubDirectory(blink::mojom::ShareError error);
  void OnStoreFiles(blink::mojom::ShareError error);
  void OnShowSharePicker(blink::mojom::ShareError error);

  static void ShowSharePicker(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      blink::mojom::ShareService::ShareCallback close_callback);
  static SharePickerCallback& GetSharePickerCallback();

  const std::string title_;
  const std::string text_;
  const GURL url_;
  std::vector<blink::mojom::SharedFilePtr> shared_files_;
  blink::mojom::ShareService::ShareCallback callback_;

  std::unique_ptr<PrepareDirectoryTask> prepare_directory_task_;
  std::unique_ptr<PrepareSubDirectoryTask> prepare_subdirectory_task_;

  base::FilePath directory_;
  std::vector<base::FilePath> file_paths_;

  base::WeakPtrFactory<SharingServiceOperation> weak_factory_{this};
};
}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_MAC_SHARING_SERVICE_OPERATION_H_
