// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
struct GlobalRequestID;
}  // namespace content

namespace views {

// A kind of webview that can notify its delegate when its content is ready.
class ObservableWebView : public WebView {
 public:
  ObservableWebView(content::BrowserContext* browser_context,
                    ui::WebDialogDelegate* delegate);
  ~ObservableWebView() override;

  // content::WebContentsObserver
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;

  // Resets the delegate. The delegate will no longer receive calls after this
  // point.
  void ResetDelegate();

 private:
  ui::WebDialogDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ObservableWebView);
};

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
class WEBVIEW_EXPORT WebDialogView : public ClientView,
                                     public ui::WebDialogWebContentsDelegate,
                                     public ui::WebDialogDelegate,
                                     public WidgetDelegate {
 public:
  // |handler| must not be nullptr.
  // |use_dialog_frame| indicates whether to use dialog frame view for non
  // client frame view.
  WebDialogView(content::BrowserContext* context,
                ui::WebDialogDelegate* delegate,
                std::unique_ptr<WebContentsHandler> handler,
                bool use_dialog_frame = false);
  ~WebDialogView() override;

  content::WebContents* web_contents();

  // ClientView:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  bool CanClose() override;

  // WidgetDelegate:
  bool OnCloseRequested(Widget::ClosedReason close_reason) override;
  bool CanResize() const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetAccessibleWindowTitle() const override;
  std::string GetWindowName() const override;
  void WindowClosing() override;
  View* GetContentsView() override;
  ClientView* CreateClientView(Widget* widget) override;
  NonClientFrameView* CreateNonClientFrameView(Widget* widget) override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  void GetMinimumDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnDialogCloseFromWebUI(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCenterDialogTitleText() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // content::WebContentsDelegate:
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void CloseContents(content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void BeforeUnloadFired(content::WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;

 private:
  friend class WebDialogViewUnitTest;
  FRIEND_TEST_ALL_PREFIXES(WebDialogBrowserTest, WebContentRendered);

  // Initializes the contents of the dialog.
  void InitDialog();

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  ui::WebDialogDelegate* delegate_;

  ObservableWebView* web_view_;

  // Whether user is attempting to close the dialog and we are processing
  // beforeunload event.
  bool is_attempting_close_dialog_ = false;

  // Whether beforeunload event has been fired and we have finished processing
  // beforeunload event.
  bool before_unload_fired_ = false;

  // Whether the dialog is closed from WebUI in response to a "dialogClose"
  // message.
  bool closed_via_webui_ = false;

  // A json string returned to WebUI from a "dialogClose" message.
  std::string dialog_close_retval_;

  // Whether CloseContents() has been called.
  bool close_contents_called_ = false;

  // Handler for unhandled key events from renderer.
  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Whether to use dialog frame view for non client frame view.
  bool use_dialog_frame_ = false;

  bool disable_url_load_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebDialogView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEB_DIALOG_VIEW_H_
