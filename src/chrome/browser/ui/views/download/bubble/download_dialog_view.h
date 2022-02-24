// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

class Browser;

class DownloadDialogView : public views::View {
 public:
  METADATA_HEADER(DownloadDialogView);
  DownloadDialogView(const DownloadDialogView&) = delete;
  DownloadDialogView& operator=(const DownloadDialogView&) = delete;

  DownloadDialogView(raw_ptr<Browser> browser,
                     std::unique_ptr<DownloadBubbleRowListView> row_list_view);

 private:
  void CloseBubble();
  void AddHeader();
  void AddFooter();
  void ShowAllDownloads();
  void OnThemeChanged() override;

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<views::Link> footer_link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
