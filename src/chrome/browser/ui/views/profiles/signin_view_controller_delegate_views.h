// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class GURL;
struct AccountInfo;

namespace content {
class WebContents;
class WebContentsDelegate;
}

namespace signin_metrics {
enum class ReauthAccessPoint;
}

namespace views {
class WebView;
}

// Views implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they're managing
// closes (in the DeleteDelegate callback).
class SigninViewControllerDelegateViews
    : public views::DialogDelegateView,
      public SigninViewControllerDelegate,
      public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate {
 public:
  METADATA_HEADER(SigninViewControllerDelegateViews);

  SigninViewControllerDelegateViews(const SigninViewControllerDelegateViews&) =
      delete;
  SigninViewControllerDelegateViews& operator=(
      const SigninViewControllerDelegateViews&) = delete;

  static std::unique_ptr<views::WebView> CreateSyncConfirmationWebView(
      Browser* browser);

  static std::unique_ptr<views::WebView> CreateSigninErrorWebView(
      Browser* browser);

  static std::unique_ptr<views::WebView> CreateReauthConfirmationWebView(
      Browser* browser,
      signin_metrics::ReauthAccessPoint);

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  static std::unique_ptr<views::WebView> CreateEnterpriseConfirmationWebView(
      Browser* browser,
      const AccountInfo& account_info,
      SkColor profile_color,
      base::OnceCallback<void(bool)> callback);
#endif

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool ShouldShowCloseButton() const override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

 private:
  friend SigninViewControllerDelegate;
  friend class SigninViewControllerDelegateViewsBrowserTest;

  // Creates and displays a constrained window containing |web_contents|. If
  // |wait_for_size| is true, the delegate will wait for ResizeNativeView() to
  // be called by the base class before displaying the constrained window.
  SigninViewControllerDelegateViews(
      std::unique_ptr<views::WebView> content_view,
      Browser* browser,
      ui::ModalType dialog_modal_type,
      bool wait_for_size,
      bool should_show_close_button);
  ~SigninViewControllerDelegateViews() override;

  // Creates a WebView for a dialog with the specified URL.
  static std::unique_ptr<views::WebView> CreateDialogWebView(
      Browser* browser,
      const GURL& url,
      int dialog_height,
      absl::optional<int> dialog_width);

  // Displays the modal dialog.
  void DisplayModal();

  // This instance of `SigninViewControllerDelegateViews` is initially
  // self-owned and then passes ownership of itself and the content view to
  // `modal_signin_widget_` and becomes owned by the view hierarchy.
  std::unique_ptr<views::WebView> owned_content_view_;

  // If the widget is non-null, then it owns the
  // `SigninViewControllerDelegateViews` and the content view.
  views::Widget* modal_signin_widget_ = nullptr;

  content::WebContents* web_contents_;
  Browser* const browser_;
  views::WebView* content_view_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  bool should_show_close_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
