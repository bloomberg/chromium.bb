// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace views {
class StyledLabel;
class View;
}  // namespace views

enum class SharingDialogType;

class SharingDialogView : public SharingDialog,
                          public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SharingDialogView(views::View* anchor_view,
                    content::WebContents* web_contents,
                    SharingDialogData data);

  ~SharingDialogView() override;

  // SharingDialog:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  void WebContentsDestroyed() override;
  void AddedToWidget() override;

  static views::BubbleDialogDelegateView* GetAsBubble(SharingDialog* dialog);
  static views::BubbleDialogDelegateView* GetAsBubbleForClickToCall(
      SharingDialog* dialog);

  SharingDialogType GetDialogType() const;

  const View* button_list_for_testing() const { return button_list_; }

 private:
  friend class SharingDialogViewTest;

  // LocationBarBubbleDelegateView:
  void Init() override;

  // Populates the dialog view containing valid devices and apps.
  void InitListView();
  // Populates the dialog view containing error help text.
  void InitErrorView();

  std::unique_ptr<views::StyledLabel> CreateHelpText();

  void DeviceButtonPressed(size_t index);
  void AppButtonPressed(size_t index);

  SharingDialogData data_;

  // References to device and app buttons views.
  View* button_list_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharingDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
