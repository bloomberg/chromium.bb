// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_

#include "content/common/content_export.h"

namespace download {
class DownloadItem;
}

namespace content {

class BrowserContext;
class WebContents;

// Helper class to attach WebContents and BrowserContext to a DownloadItem.
class CONTENT_EXPORT DownloadItemUtils {
 public:
  // Helper method to get the BrowserContext from a DownloadItem.
  static BrowserContext* GetBrowserContext(
      const download::DownloadItem* downloadItem);

  // Helper method to get the WebContents from a DownloadItem.
  static WebContents* GetWebContents(
      const download::DownloadItem* downloadItem);

  // Attach information to a DownloadItem.
  static void AttachInfo(download::DownloadItem* downloadItem,
                         BrowserContext* browser_context,
                         WebContents* web_contents);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_
