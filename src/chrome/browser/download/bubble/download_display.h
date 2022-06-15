// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_

class DownloadDisplay {
 public:
  // Shows the download display.
  virtual void Show() = 0;
  // Hides the download display.
  virtual void Hide() = 0;
  // Returns whether or not the download display is visible.
  virtual bool IsShowing() = 0;
  // Enables potential actions resulting from clicking the download display.
  virtual void Enable() = 0;
  // Disables potential actions resulting from clicking the download display.
  virtual void Disable() = 0;
  // Updates the download icon.
  virtual void UpdateDownloadIcon() = 0;
  // Shows detailed information on the download display. It can be a popup or
  // dialog or partial view, essentially anything other than the main view.
  virtual void ShowDetails() = 0;
  // Hide the detailed information on the download display.
  virtual void HideDetails() = 0;
  // Returns whether the details are visible.
  virtual bool IsShowingDetails() = 0;

 protected:
  virtual ~DownloadDisplay();
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
