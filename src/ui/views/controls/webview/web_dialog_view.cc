// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_dialog_view.h"

#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;
using ui::WebDialogUIBase;
using ui::WebDialogWebContentsDelegate;

namespace views {

ObservableWebView::ObservableWebView(content::BrowserContext* browser_context,
                                     WebDialogDelegate* delegate)
    : WebView(browser_context), delegate_(delegate) {}

ObservableWebView::~ObservableWebView() = default;

void ObservableWebView::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Only listen to the main frame.
  if (render_frame_host->GetParent())
    return;

  if (delegate_)
    delegate_->OnWebContentsFinishedLoad();
}

void ObservableWebView::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  // Only listen to the main frame.
  if (render_frame_host->GetParent())
    return;

  if (delegate_)
    delegate_->OnMainFrameResourceLoadComplete(resource_load_info);
}

void ObservableWebView::ResetDelegate() {
  delegate_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, public:

WebDialogView::WebDialogView(content::BrowserContext* context,
                             WebDialogDelegate* delegate,
                             std::unique_ptr<WebContentsHandler> handler,
                             bool use_dialog_frame)
    : ClientView(nullptr, nullptr),
      WebDialogWebContentsDelegate(context, std::move(handler)),
      delegate_(delegate),
      web_view_(new ObservableWebView(context, delegate)),
      use_dialog_frame_(use_dialog_frame) {
  web_view_->set_allow_accelerators(true);
  AddChildView(web_view_);
  set_contents_view(web_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Pressing the Escape key will close the dialog.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  if (delegate_) {
    for (const auto& accelerator : delegate_->GetAccelerators())
      AddAccelerator(accelerator);
  }
}

WebDialogView::~WebDialogView() = default;

content::WebContents* WebDialogView::web_contents() {
  return web_view_->web_contents();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, views::View implementation:

gfx::Size WebDialogView::CalculatePreferredSize() const {
  gfx::Size out;
  if (delegate_)
    delegate_->GetDialogSize(&out);
  return out;
}

gfx::Size WebDialogView::GetMinimumSize() const {
  gfx::Size out;
  if (delegate_)
    delegate_->GetMinimumDialogSize(&out);
  return out;
}

bool WebDialogView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (delegate_ && delegate_->AcceleratorPressed(accelerator))
    return true;

  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  if (delegate_ && !delegate_->ShouldCloseDialogOnEscape())
    return false;

  // Pressing Escape closes the dialog.
  if (GetWidget()) {
    // Contents must be closed first, or the dialog will not close.
    CloseContents(web_view_->web_contents());
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  }
  return true;
}

void WebDialogView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && GetWidget())
    InitDialog();
}

bool WebDialogView::CanClose() {
  // Don't close UI if |delegate_| does not allow users to close it by
  // clicking on "x" button or pressing Escape shortcut key on hosting
  // dialog.
  if (!delegate_->CanCloseDialog() && !close_contents_called_)
    return false;

  // If CloseContents() is called before CanClose(), which is called by
  // RenderViewHostImpl::ClosePageIgnoringUnloadEvents, it indicates
  // beforeunload event should not be fired during closing.
  if ((is_attempting_close_dialog_ && before_unload_fired_) ||
      close_contents_called_) {
    is_attempting_close_dialog_ = false;
    before_unload_fired_ = false;
    return true;
  }

  if (!is_attempting_close_dialog_) {
    // Fire beforeunload event when user attempts to close the dialog.
    is_attempting_close_dialog_ = true;
    web_view_->web_contents()->DispatchBeforeUnload(false /* auto_cancel */);
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, views::WidgetDelegate implementation:

bool WebDialogView::OnCloseRequested(Widget::ClosedReason close_reason) {
  return !delegate_ || delegate_->OnDialogCloseRequested();
}

bool WebDialogView::CanResize() const {
  if (delegate_)
    return delegate_->CanResizeDialog();
  return true;
}

ui::ModalType WebDialogView::GetModalType() const {
  return GetDialogModalType();
}

base::string16 WebDialogView::GetWindowTitle() const {
  if (delegate_)
    return delegate_->GetDialogTitle();
  return base::string16();
}

base::string16 WebDialogView::GetAccessibleWindowTitle() const {
  if (delegate_)
    return delegate_->GetAccessibleDialogTitle();
  return GetWindowTitle();
}

std::string WebDialogView::GetWindowName() const {
  if (delegate_)
    return delegate_->GetDialogName();
  return std::string();
}

void WebDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

views::View* WebDialogView::GetContentsView() {
  return this;
}

views::ClientView* WebDialogView::CreateClientView(views::Widget* widget) {
  return this;
}

NonClientFrameView* WebDialogView::CreateNonClientFrameView(Widget* widget) {
  if (use_dialog_frame_)
    return DialogDelegate::CreateDialogFrameView(widget);
  return WidgetDelegate::CreateNonClientFrameView(widget);
}

views::View* WebDialogView::GetInitiallyFocusedView() {
  return web_view_;
}

bool WebDialogView::ShouldShowWindowTitle() const {
  return ShouldShowDialogTitle();
}

views::Widget* WebDialogView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* WebDialogView::GetWidget() const {
  return View::GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogDelegate implementation:

ui::ModalType WebDialogView::GetDialogModalType() const {
  if (delegate_)
    return delegate_->GetDialogModalType();
  return ui::MODAL_TYPE_NONE;
}

base::string16 WebDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL WebDialogView::GetDialogContentURL() const {
  if (delegate_)
    return delegate_->GetDialogContentURL();
  return GURL();
}

void WebDialogView::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_)
    delegate_->GetWebUIMessageHandlers(handlers);
}

void WebDialogView::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
}

void WebDialogView::GetMinimumDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetMinimumDialogSize(size);
}

std::string WebDialogView::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  return std::string();
}

void WebDialogView::OnDialogShown(content::WebUI* webui) {
  if (delegate_)
    delegate_->OnDialogShown(webui);
}

void WebDialogView::OnDialogClosed(const std::string& json_retval) {
  Detach();
  if (delegate_) {
    // Store the dialog content area size.
    delegate_->StoreDialogSize(GetContentsBounds().size());
  }

  if (GetWidget())
    GetWidget()->Close();

  if (delegate_) {
    delegate_->OnDialogClosed(json_retval);
    delegate_ = nullptr;  // We will not communicate further with the delegate.
    // Clear the copy of the delegate in |web_view_| too.
    web_view_->ResetDelegate();
  }
}

void WebDialogView::OnDialogCloseFromWebUI(const std::string& json_retval) {
  closed_via_webui_ = true;
  dialog_close_retval_ = json_retval;
  if (GetWidget())
    GetWidget()->Close();
}

void WebDialogView::OnCloseContents(WebContents* source,
                                    bool* out_close_dialog) {
  DCHECK(out_close_dialog);
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

bool WebDialogView::ShouldShowDialogTitle() const {
  if (delegate_)
    return delegate_->ShouldShowDialogTitle();
  return true;
}

bool WebDialogView::ShouldCenterDialogTitleText() const {
  if (delegate_)
    return delegate_->ShouldCenterDialogTitleText();
  return false;
}

bool WebDialogView::ShouldShowCloseButton() const {
  if (delegate_)
    return delegate_->ShouldShowCloseButton();
  return true;
}

bool WebDialogView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  if (delegate_)
    return delegate_->HandleContextMenu(render_frame_host, params);
  return WebDialogWebContentsDelegate::HandleContextMenu(render_frame_host,
                                                         params);
}

////////////////////////////////////////////////////////////////////////////////
// content::WebContentsDelegate implementation:

void WebDialogView::SetContentsBounds(WebContents* source,
                                      const gfx::Rect& bounds) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  GetWidget()->SetBounds(bounds);
}

// A simplified version of BrowserView::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
bool WebDialogView::HandleKeyboardEvent(content::WebContents* source,
                                        const NativeWebKeyboardEvent& event) {
  if (!event.os_event)
    return false;

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void WebDialogView::CloseContents(WebContents* source) {
  close_contents_called_ = true;
  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(closed_via_webui_ ? dialog_close_retval_ : std::string());
}

content::WebContents* WebDialogView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* new_contents = nullptr;
  if (delegate_ &&
      delegate_->HandleOpenURLFromTab(source, params, &new_contents)) {
    return new_contents;
  }
  return WebDialogWebContentsDelegate::OpenURLFromTab(source, params);
}

void WebDialogView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  WebDialogWebContentsDelegate::AddNewContents(
      source, std::move(new_contents), target_url, disposition, initial_rect,
      user_gesture, was_blocked);
}

void WebDialogView::LoadingStateChanged(content::WebContents* source,
                                        bool to_different_document) {
  if (delegate_)
    delegate_->OnLoadingStateChanged(source);
}

void WebDialogView::BeforeUnloadFired(content::WebContents* tab,
                                      bool proceed,
                                      bool* proceed_to_fire_unload) {
  before_unload_fired_ = true;
  *proceed_to_fire_unload = proceed;
}

bool WebDialogView::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  if (delegate_)
    return delegate_->HandleShouldOverrideWebContentsCreation();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, private:

void WebDialogView::InitDialog() {
  content::WebContents* web_contents = web_view_->GetWebContents();
  if (web_contents->GetDelegate() == this)
    return;

  web_contents->SetDelegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above WebDialogUI in its header file for why.
  WebDialogUIBase::SetDelegate(web_contents, this);

  if (!disable_url_load_for_test_)
    web_view_->LoadInitialURL(GetDialogContentURL());
}

}  // namespace views
