// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace permissions {
enum class RequestType;
enum class PermissionAction;
}  // namespace permissions

namespace view {
class Widget;
}

namespace view {
class Widget;
}

class Browser;

// Bubble that prompts the user to grant or deny a permission request from a
// website.
class PermissionPromptBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(PermissionPromptBubbleView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPermissionPromptBubbleViewIdentifier);
  PermissionPromptBubbleView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style);
  PermissionPromptBubbleView(const PermissionPromptBubbleView&) = delete;
  PermissionPromptBubbleView& operator=(const PermissionPromptBubbleView&) =
      delete;
  ~PermissionPromptBubbleView() override;

  void Show();

  // Anchors the bubble to the view or rectangle returned from
  // bubble_anchor_util::GetPageInfoAnchorConfiguration.
  void UpdateAnchorPosition();

  void SetPromptStyle(PermissionPromptStyle prompt_style);

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;

  void AcceptPermission();
  void AcceptPermissionThisTime();
  void DenyPermission();
  void ClosingPermission();

 private:
  void AddRequestLine(permissions::PermissionRequest* request);

  // Record UMA Permissions.*.TimeToDecision.|action| metric. Can be
  // Permissions.Prompt.TimeToDecision.* or Permissions.Chip.TimeToDecision.*,
  // depending on which UI is used.
  void RecordDecision(permissions::PermissionAction action);

  const raw_ptr<Browser> browser_;
  base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate_;

  base::TimeTicks permission_requested_time_;

  PermissionPromptStyle prompt_style_;

  const std::u16string display_name_;
  const std::u16string accessible_window_title_;
  const std::u16string window_title_;
  const bool is_display_name_origin_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
