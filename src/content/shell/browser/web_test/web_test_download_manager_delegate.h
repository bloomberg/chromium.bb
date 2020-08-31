// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/shell/browser/shell_download_manager_delegate.h"

namespace download {
class DownloadItem;
}

namespace content {

class WebTestDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  WebTestDownloadManagerDelegate();
  ~WebTestDownloadManagerDelegate() override;

  // ShellDownloadManagerDelegate implementation.
  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback callback) override;
  void CheckDownloadAllowed(
      const content::WebContents::Getter& web_contents_getter,
      const GURL& url,
      const std::string& request_method,
      base::Optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      bool content_initiated,
      content::CheckDownloadAllowedCallback check_download_allowed_cb) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestDownloadManagerDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_
