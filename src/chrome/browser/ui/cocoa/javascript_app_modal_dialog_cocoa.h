// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_

#include <memory>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "ui/views_bridge_mac/alert.h"
#include "ui/views_bridge_mac/mojo/alert.mojom.h"

class PopunderPreventer;

namespace app_modal {
class JavaScriptAppModalDialog;
}

class JavaScriptAppModalDialogCocoa : public app_modal::NativeAppModalDialog {
 public:
  explicit JavaScriptAppModalDialogCocoa(
      app_modal::JavaScriptAppModalDialog* dialog);

  // Overridden from NativeAppModalDialog:
  int GetAppModalDialogButtons() const override;
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  app_modal::JavaScriptAppModalDialog* dialog() const {
    return dialog_.get();
  }

 private:
  ~JavaScriptAppModalDialogCocoa() override;

  // Return the parameters to use for the alert.
  views_bridge_mac::mojom::AlertBridgeInitParamsPtr GetAlertParams();

  // Called when the alert completes. Deletes |this|.
  void OnAlertFinished(views_bridge_mac::mojom::AlertDisposition disposition,
                       const base::string16& prompt_text,
                       bool suppress_js_messages);

  // Called if there is an error connecting to the alert process. Deletes
  // |this|.
  void OnConnectionError();

  // Mojo interface to the NSAlert.
  views_bridge_mac::mojom::AlertBridgePtr alert_bridge_;

  std::unique_ptr<app_modal::JavaScriptAppModalDialog> dialog_;
  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  int num_buttons_ = 0;
  bool is_showing_ = false;

  base::WeakPtrFactory<JavaScriptAppModalDialogCocoa> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialogCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
