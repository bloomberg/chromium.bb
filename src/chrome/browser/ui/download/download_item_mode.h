// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_

class DownloadUIModel;

namespace download {

// Security UI mode of a download item.
enum class DownloadItemMode {
  kNormal,             // Showing download item.
  kDangerous,          // Displaying the dangerous download warning.
  kMalicious,          // Displaying the malicious download warning.
  kMixedContentWarn,   // Displaying the mixed-content download warning.
  kMixedContentBlock,  // Displaying the mixed-content download block error.
  kDeepScanning,       // Displaying in-progress deep scanning information.
  kIncognitoWarning,  // Displaying warning about files saved on the device even
                      // in Incognito.
};

// Returns the mode that best reflects the current model state.
DownloadItemMode GetDesiredDownloadItemMode(DownloadUIModel* download);

}  // namespace download

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_
