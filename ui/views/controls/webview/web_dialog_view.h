// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class WebView;

////////////////////////////////////////////////////////////////////////////////
//
// WebDialogView is a view used to display an web dialog to the user. The
// content of the dialogs is determined by the delegate
// (ui::WebDialogDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
//
// TODO(akalin): Make WebDialogView contain an WebDialogWebContentsDelegate
// instead of inheriting from it to avoid violating the "no multiple
// inheritance" rule.
class WEBVIEW_EXPORT WebDialogView : public views::ClientView,
                                     public ui::WebDialogWebContentsDelegate,
                                     public ui::WebDialogDelegate,
                                     public views::WidgetDelegate {
 public:
  // |handler| must not be NULL and this class takes the ownership.
  WebDialogView(content::BrowserContext* context,
                ui::WebDialogDelegate* delegate,
                WebContentsHandler* handler);
  virtual ~WebDialogView();

  // For testing.
  content::WebContents* web_contents();

  // Overridden from views::ClientView:
  virtual gfx::Size GetPreferredSize() const override;
  virtual gfx::Size GetMinimumSize() const override;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator)
      override;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  virtual bool CanClose() override;

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const override;
  virtual ui::ModalType GetModalType() const override;
  virtual base::string16 GetWindowTitle() const override;
  virtual std::string GetWindowName() const override;
  virtual void WindowClosing() override;
  virtual views::View* GetContentsView() override;
  virtual ClientView* CreateClientView(views::Widget* widget) override;
  virtual views::View* GetInitiallyFocusedView() override;
  virtual bool ShouldShowWindowTitle() const override;
  virtual views::Widget* GetWidget() override;
  virtual const views::Widget* GetWidget() const override;

  // Overridden from ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const override;
  virtual base::string16 GetDialogTitle() const override;
  virtual GURL GetDialogContentURL() const override;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  virtual void GetDialogSize(gfx::Size* size) const override;
  virtual void GetMinimumDialogSize(gfx::Size* size) const override;
  virtual std::string GetDialogArgs() const override;
  virtual void OnDialogShown(
      content::WebUI* webui,
      content::RenderViewHost* render_view_host) override;
  virtual void OnDialogClosed(const std::string& json_retval) override;
  virtual void OnDialogCloseFromWebUI(
      const std::string& json_retval) override;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) override;
  virtual bool ShouldShowDialogTitle() const override;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) override;

  // Overridden from content::WebContentsDelegate:
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) override;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  virtual void CloseContents(content::WebContents* source) override;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) override;
  virtual void LoadingStateChanged(content::WebContents* source,
                                   bool to_different_document) override;
  virtual void BeforeUnloadFired(content::WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebDialogBrowserTest, WebContentRendered);

  // Initializes the contents of the dialog.
  void InitDialog();

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  ui::WebDialogDelegate* delegate_;

  views::WebView* web_view_;

  // Whether user is attempting to close the dialog and we are processing
  // beforeunload event.
  bool is_attempting_close_dialog_;

  // Whether beforeunload event has been fired and we have finished processing
  // beforeunload event.
  bool before_unload_fired_;

  // Whether the dialog is closed from WebUI in response to a "dialogClose"
  // message.
  bool closed_via_webui_;

  // A json string returned to WebUI from a "dialogClose" message.
  std::string dialog_close_retval_;

  // Whether CloseContents() has been called.
  bool close_contents_called_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_
