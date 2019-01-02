// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"

// Contains various utility methods for conversions between DownloadItem and
// OfflineItem.
class OfflineItemUtils {
 public:
  static offline_items_collection::OfflineItem CreateOfflineItem(
      download::DownloadItem* item);

  static std::string GetDownloadNamespace(bool is_off_the_record);

 private:
  DISALLOW_COPY_AND_ASSIGN(OfflineItemUtils);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_
