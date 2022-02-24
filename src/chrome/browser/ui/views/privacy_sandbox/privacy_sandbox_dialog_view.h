// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

namespace views {
class WebView;
}

// Implements the PrivacySandboxDialog as a View. The view contains a WebView
// into which is loaded a WebUI page which renders the actual dialog content.
class PrivacySandboxDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(PrivacySandboxDialogView);
  PrivacySandboxDialogView(Browser* browser,
                           PrivacySandboxService::DialogType dialog_type);

  void Close();

 private:
  void ResizeNativeView(int height);
  void OpenPrivacySandboxSettings();

  raw_ptr<views::WebView> web_view_;
  raw_ptr<Browser> browser_;
  base::TimeTicks dialog_created_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
