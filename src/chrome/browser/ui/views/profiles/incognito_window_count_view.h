// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

// The implementation for a bubble that allows the user to close all open
// incognito windows.
// The IncognitoWindowCountView is self-owned and deletes itself when it is
// closed or the parent browser is being destroyed.
class IncognitoWindowCountView : public views::BubbleDialogDelegateView,
                                 public BrowserListObserver {
 public:
  static void ShowBubble(views::Button* anchor_button,
                         Browser* browser,
                         int incognito_window_count);

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Accept() override;
  bool Close() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowIcon() const override;
  gfx::ImageSkia GetWindowIcon() override;
  void Init() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  IncognitoWindowCountView(views::Button* anchor_button,
                           Browser* browser,
                           int incognito_window_count);

  ~IncognitoWindowCountView() override;

  int incognito_window_count_;
  Browser* const browser_;

  ScopedObserver<BrowserList, BrowserListObserver> browser_list_observer_;
  base::WeakPtrFactory<IncognitoWindowCountView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowCountView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_
