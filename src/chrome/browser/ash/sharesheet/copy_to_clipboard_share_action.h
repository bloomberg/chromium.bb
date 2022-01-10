// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_
#define CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_

#include "chrome/browser/sharesheet/share_action/share_action.h"

class Profile;

namespace ash {

struct ToastData;

namespace sharesheet {

class CopyToClipboardShareAction : public ::sharesheet::ShareAction {
 public:
  explicit CopyToClipboardShareAction(Profile* profile);
  ~CopyToClipboardShareAction() override;
  CopyToClipboardShareAction(const CopyToClipboardShareAction&) = delete;
  CopyToClipboardShareAction& operator=(const CopyToClipboardShareAction&) =
      delete;

  // ShareAction:
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(::sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(::sharesheet::SharesheetController* controller) override;
  bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                        bool contains_hosted_document) override;

 private:
  // Virtual so that it can be overridden in testing.
  virtual void ShowToast(const ash::ToastData& toast_data);

  Profile* profile_;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_
