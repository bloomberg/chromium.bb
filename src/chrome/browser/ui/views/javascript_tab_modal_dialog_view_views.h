// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

// A Views version of a JavaScript dialog that automatically dismisses itself
// when the user switches away to a different tab, used for WebContentses that
// are browser tabs.
class JavaScriptTabModalDialogViewViews
    : public javascript_dialogs::TabModalDialogView,
      public views::DialogDelegateView {
 public:
  ~JavaScriptTabModalDialogViewViews() override;

  // JavaScriptDialog:
  void CloseDialogWithoutCallback() override;
  base::string16 GetUserInput() override;

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  views::View* GetInitiallyFocusedView() override;
  ui::ModalType GetModalType() const override;

  // views::View:
  void AddedToWidget() override;

 private:
  friend class JavaScriptDialog;
  friend class JavaScriptTabModalDialogManagerDelegateDesktop;

  JavaScriptTabModalDialogViewViews(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_force_closed_callback);

  base::string16 title_;
  base::string16 message_text_;
  base::string16 default_prompt_text_;
  content::JavaScriptDialogManager::DialogClosedCallback dialog_callback_;
  base::OnceClosure dialog_force_closed_callback_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  base::WeakPtrFactory<JavaScriptTabModalDialogViewViews> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JavaScriptTabModalDialogViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_
