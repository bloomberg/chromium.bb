// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class DiagnosticsDialog : public SystemWebDialogDelegate {
 public:
  // Denotes different sub-pages of the diagnostics app.
  enum class DiagnosticsPage {
    // The default page.
    kDefault,
    // The system page.
    kSystem,
    // The connectivity page.
    kConnectivity,
    // The input page.
    kInput
  };

  // |page| is the initial page shown when the app is opened.
  static void ShowDialog(DiagnosticsPage page = DiagnosticsPage::kDefault,
                         gfx::NativeWindow parent = gfx::kNullNativeWindow);

 protected:
  explicit DiagnosticsDialog(DiagnosticsPage page);
  ~DiagnosticsDialog() override;

  DiagnosticsDialog(const DiagnosticsDialog&) = delete;
  DiagnosticsDialog& operator=(const DiagnosticsDialog&) = delete;

  // SystemWebDialogDelegate
  const std::string& Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

 private:
  const std::string id_ = "diagnostics-dialog";
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_
