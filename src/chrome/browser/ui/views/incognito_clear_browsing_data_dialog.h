// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace views {
class View;
}  // namespace views

class IncognitoClearBrowsingDataDialog
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(IncognitoClearBrowsingDataDialog);

  enum Type {
    kDefaultBubble = 0,
    kHistoryDisclaimerBubble = 1,
    kMaxValue = kHistoryDisclaimerBubble,
  };

  static void Show(views::View* anchor_view,
                   Profile* incognito_profile,
                   Type type);
  static bool IsShowing();

  // testing
  static IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();
  void SetDestructorCallbackForTesting(base::OnceClosure callback);

  IncognitoClearBrowsingDataDialog(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  IncognitoClearBrowsingDataDialog& operator=(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  ~IncognitoClearBrowsingDataDialog() override;

 private:
  explicit IncognitoClearBrowsingDataDialog(views::View* anchor_view,
                                            Profile* incognito_profile,
                                            Type type);

  static void CloseDialog();

  // Helper methods to add functionality to the button.
  void OnCloseWindowsButtonClicked();
  void OnCancelButtonClicked();

  void SetDialogForDefaultBubbleType();
  void SetDialogForHistoryDisclaimerBubbleType();

  raw_ptr<Profile> incognito_profile_;
  base::OnceClosure destructor_callback_ = base::DoNothing();
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
