// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_background_host.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_view_host.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/url_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/process_manager.h"
#include "pdf/pdf_features.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace {

printing::CefPrintViewManager* GetPrintViewManager(
    content::WebContents* web_contents) {
  return printing::CefPrintViewManager::FromWebContents(web_contents);
}

}  // namespace

CefBrowserPlatformDelegateAlloy::CefBrowserPlatformDelegateAlloy()
    : weak_ptr_factory_(this) {}

content::WebContents* CefBrowserPlatformDelegateAlloy::CreateWebContents(
    CefBrowserCreateParams& create_params,
    bool& own_web_contents) {
  REQUIRE_ALLOY_RUNTIME();
  DCHECK(primary_);

  // Get or create the request context and browser context.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params.request_context);
  CHECK(request_context_impl);
  auto cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  auto browser_context = cef_browser_context->AsBrowserContext();

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = request_context_impl.get();
  }

  scoped_refptr<content::SiteInstance> site_instance;
  if (extensions::ExtensionsEnabled() && !create_params.url.empty()) {
    GURL gurl = url_util::MakeGURL(create_params.url, /*fixup=*/true);
    if (!create_params.extension) {
      // We might be loading an extension app view where the extension URL is
      // provided by the client.
      create_params.extension =
          extensions::GetExtensionForUrl(browser_context, gurl);
    }
    if (create_params.extension) {
      if (create_params.extension_host_type ==
          extensions::mojom::ViewType::kInvalid) {
        // Default to dialog behavior.
        create_params.extension_host_type =
            extensions::mojom::ViewType::kExtensionDialog;
      }

      // Extension resources will fail to load if we don't use a SiteInstance
      // associated with the extension.
      // (AlloyContentBrowserClient::SiteInstanceGotProcess won't find the
      // extension to register with InfoMap, and AllowExtensionResourceLoad in
      // ExtensionProtocolHandler::MaybeCreateJob will return false resulting in
      // ERR_BLOCKED_BY_CLIENT).
      site_instance = extensions::ProcessManager::Get(browser_context)
                          ->GetSiteInstanceForURL(gurl);
      DCHECK(site_instance);
    }
  }

  content::WebContents::CreateParams wc_create_params(browser_context,
                                                      site_instance);

  if (IsWindowless()) {
    // Create the OSR view for the WebContents.
    CreateViewForWebContents(&wc_create_params.view,
                             &wc_create_params.delegate_view);
  }

  auto web_contents = content::WebContents::Create(wc_create_params);
  CHECK(web_contents);

  own_web_contents = true;
  return web_contents.release();
}

void CefBrowserPlatformDelegateAlloy::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegate::WebContentsCreated(web_contents, owned);

  if (primary_) {
    find_in_page::FindTabHelper::CreateForWebContents(web_contents);

    if (owned) {
      SetOwnedWebContents(web_contents);
    }
  } else {
    DCHECK(!owned);
  }
}

void CefBrowserPlatformDelegateAlloy::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  REQUIRE_ALLOY_RUNTIME();
  DCHECK(primary_);

  CefRefPtr<AlloyBrowserHostImpl> owner =
      AlloyBrowserHostImpl::GetBrowserForContents(new_contents.get());
  if (owner) {
    // Taking ownership of |new_contents|.
    static_cast<CefBrowserPlatformDelegateAlloy*>(
        owner->platform_delegate_.get())
        ->SetOwnedWebContents(new_contents.release());
    return;
  }

  if (extension_host_) {
    extension_host_->AddNewContents(source, std::move(new_contents), target_url,
                                    disposition, initial_rect, user_gesture,
                                    was_blocked);
  }
}

bool CefBrowserPlatformDelegateAlloy::
    ShouldAllowRendererInitiatedCrossProcessNavigation(
        bool is_main_frame_navigation) {
  if (extension_host_) {
    return extension_host_->ShouldAllowRendererInitiatedCrossProcessNavigation(
        is_main_frame_navigation);
  }
  return true;
}

void CefBrowserPlatformDelegateAlloy::RenderViewReady() {
  ConfigureAutoResize();
}

void CefBrowserPlatformDelegateAlloy::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegate::BrowserCreated(browser);

  // Only register WebContents delegate/observers if we're the primary delegate.
  if (!primary_)
    return;

  DCHECK(!web_contents_->GetDelegate());
  web_contents_->SetDelegate(static_cast<AlloyBrowserHostImpl*>(browser));

  PrefsTabHelper::CreateForWebContents(web_contents_);
  printing::CefPrintViewManager::CreateForWebContents(web_contents_);

  if (extensions::ExtensionsEnabled()) {
    // Used by the tabs extension API.
    zoom::ZoomController::CreateForWebContents(web_contents_);
  }

  if (IsPrintPreviewSupported()) {
    web_contents_dialog_helper_.reset(
        new CefWebContentsDialogHelper(web_contents_, this));
  }
}

void CefBrowserPlatformDelegateAlloy::CreateExtensionHost(
    const extensions::Extension* extension,
    const GURL& url,
    extensions::mojom::ViewType host_type) {
  REQUIRE_ALLOY_RUNTIME();
  DCHECK(primary_);

  // Should get WebContentsCreated and BrowserCreated calls first.
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(!extension_host_);

  auto alloy_browser = static_cast<AlloyBrowserHostImpl*>(browser_);

  if (host_type == extensions::mojom::ViewType::kExtensionDialog ||
      host_type == extensions::mojom::ViewType::kExtensionPopup) {
    // Create an extension host that we own.
    extension_host_ = new extensions::CefExtensionViewHost(
        alloy_browser, extension, web_contents_, url, host_type);
    // Trigger load of the extension URL.
    extension_host_->CreateRendererSoon();
  } else if (host_type ==
             extensions::mojom::ViewType::kExtensionBackgroundPage) {
    is_background_host_ = true;
    alloy_browser->is_background_host_ = true;
    // Create an extension host that will be owned by ProcessManager.
    extension_host_ = new extensions::CefExtensionBackgroundHost(
        alloy_browser,
        base::BindOnce(&CefBrowserPlatformDelegateAlloy::OnExtensionHostDeleted,
                       weak_ptr_factory_.GetWeakPtr()),
        extension, web_contents_, url, host_type);
    // Load will be triggered by ProcessManager::CreateBackgroundHost.
  } else {
    NOTREACHED() << " Unsupported extension host type: " << host_type;
  }
}

extensions::ExtensionHost* CefBrowserPlatformDelegateAlloy::GetExtensionHost()
    const {
  return extension_host_;
}

void CefBrowserPlatformDelegateAlloy::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  if (primary_) {
    DestroyExtensionHost();
    owned_web_contents_.reset();
  }

  CefBrowserPlatformDelegate::BrowserDestroyed(browser);
}

void CefBrowserPlatformDelegateAlloy::SendCaptureLostEvent() {
  if (!web_contents_)
    return;
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(host->GetWidget());
  if (widget)
    widget->LostCapture();
}

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
void CefBrowserPlatformDelegateAlloy::NotifyMoveOrResizeStarted() {
  if (!web_contents_)
    return;

  // Dismiss any existing popups.
  web_contents_->ClearFocusedElement();
}
#endif

bool CefBrowserPlatformDelegateAlloy::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (extension_host_)
    return extension_host_->PreHandleGestureEvent(source, event);
  return false;
}

bool CefBrowserPlatformDelegateAlloy::IsNeverComposited(
    content::WebContents* web_contents) {
  if (extension_host_)
    return extension_host_->IsNeverComposited(web_contents);
  return false;
}

void CefBrowserPlatformDelegateAlloy::SetAutoResizeEnabled(
    bool enabled,
    const CefSize& min_size,
    const CefSize& max_size) {
  if (enabled == auto_resize_enabled_)
    return;

  auto_resize_enabled_ = enabled;
  if (enabled) {
    auto_resize_min_ = gfx::Size(min_size.width, min_size.height);
    auto_resize_max_ = gfx::Size(max_size.width, max_size.height);
  } else {
    auto_resize_min_ = auto_resize_max_ = gfx::Size();
  }
  ConfigureAutoResize();
}

void CefBrowserPlatformDelegateAlloy::ConfigureAutoResize() {
  if (!web_contents_ || !web_contents_->GetRenderWidgetHostView()) {
    return;
  }

  if (auto_resize_enabled_) {
    web_contents_->GetRenderWidgetHostView()->EnableAutoResize(
        auto_resize_min_, auto_resize_max_);
  } else {
    web_contents_->GetRenderWidgetHostView()->DisableAutoResize(gfx::Size());
  }
}

void CefBrowserPlatformDelegateAlloy::SetAccessibilityState(
    cef_state_t accessibility_state) {
  // Do nothing if state is set to default. It'll be disabled by default and
  // controlled by the commmand-line flags "force-renderer-accessibility" and
  // "disable-renderer-accessibility".
  if (accessibility_state == STATE_DEFAULT)
    return;

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents_);

  if (!web_contents_impl)
    return;

  ui::AXMode accMode;
  // In windowless mode set accessibility to TreeOnly mode. Else native
  // accessibility APIs, specific to each platform, are also created.
  if (accessibility_state == STATE_ENABLED) {
    accMode = IsWindowless() ? ui::kAXModeWebContentsOnly : ui::kAXModeComplete;
  }
  web_contents_impl->SetAccessibilityMode(accMode);
}

bool CefBrowserPlatformDelegateAlloy::IsPrintPreviewSupported() const {
  REQUIRE_ALLOY_RUNTIME();

  // Print preview is not currently supported with OSR.
  if (IsWindowless())
    return false;

  auto cef_browser_context =
      CefBrowserContext::FromBrowserContext(web_contents_->GetBrowserContext());
  return cef_browser_context->IsPrintPreviewSupported();
}

void CefBrowserPlatformDelegateAlloy::Print() {
  REQUIRE_ALLOY_RUNTIME();

  auto contents_to_use = printing::GetWebContentsToUse(web_contents_);
  if (!contents_to_use)
    return;

  auto rfh_to_use = printing::GetRenderFrameHostToUse(contents_to_use);
  if (!rfh_to_use)
    return;

  if (IsPrintPreviewSupported()) {
    GetPrintViewManager(contents_to_use)->PrintPreviewNow(rfh_to_use, false);
  } else {
    GetPrintViewManager(contents_to_use)->PrintNow(rfh_to_use);
  }
}

void CefBrowserPlatformDelegateAlloy::PrintToPDF(
    const CefString& path,
    const CefPdfPrintSettings& settings,
    CefRefPtr<CefPdfPrintCallback> callback) {
  REQUIRE_ALLOY_RUNTIME();

  auto contents_to_use = printing::GetWebContentsToUse(web_contents_);
  if (!contents_to_use)
    return;

  auto rfh_to_use = printing::GetRenderFrameHostToUse(contents_to_use);
  if (!rfh_to_use)
    return;

  printing::CefPrintViewManager::PdfPrintCallback pdf_callback;
  if (callback.get()) {
    pdf_callback = base::BindOnce(&CefPdfPrintCallback::OnPdfPrintFinished,
                                  callback.get(), path);
  }
  GetPrintViewManager(contents_to_use)
      ->PrintToPDF(rfh_to_use, base::FilePath(path), settings,
                   std::move(pdf_callback));
}

void CefBrowserPlatformDelegateAlloy::Find(const CefString& searchText,
                                           bool forward,
                                           bool matchCase,
                                           bool findNext) {
  if (!web_contents_)
    return;

  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StartFinding(searchText.ToString16(), forward, matchCase, findNext,
                     /*run_synchronously_for_testing=*/false);
}

void CefBrowserPlatformDelegateAlloy::StopFinding(bool clearSelection) {
  if (!web_contents_)
    return;

  last_search_result_ = find_in_page::FindNotificationDetails();
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StopFinding(clearSelection ? find_in_page::SelectionAction::kClear
                                   : find_in_page::SelectionAction::kKeep);
}

bool CefBrowserPlatformDelegateAlloy::HandleFindReply(
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (!web_contents_)
    return false;

  auto find_in_page =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);

  find_in_page->HandleFindReply(request_id, number_of_matches, selection_rect,
                                active_match_ordinal, final_update);
  if (!(find_in_page->find_result() == last_search_result_)) {
    last_search_result_ = find_in_page->find_result();
    return true;
  }
  return false;
}

base::RepeatingClosure
CefBrowserPlatformDelegateAlloy::GetBoundsChangedCallback() {
  if (web_contents_dialog_helper_) {
    return web_contents_dialog_helper_->GetBoundsChangedCallback();
  }

  return base::RepeatingClosure();
}

void CefBrowserPlatformDelegateAlloy::SetOwnedWebContents(
    content::WebContents* owned_contents) {
  DCHECK(primary_);

  // Should not currently own a WebContents.
  CHECK(!owned_web_contents_);
  owned_web_contents_.reset(owned_contents);
}

void CefBrowserPlatformDelegateAlloy::DestroyExtensionHost() {
  if (!extension_host_)
    return;
  if (extension_host_->extension_host_type() ==
      extensions::mojom::ViewType::kExtensionBackgroundPage) {
    DCHECK(is_background_host_);
    // Close notification for background pages arrives via CloseContents.
    // The extension host will be deleted by
    // ProcessManager::CloseBackgroundHost and OnExtensionHostDeleted will be
    // called to notify us.
    extension_host_->Close();
  } else {
    DCHECK(!is_background_host_);
    // We own the extension host and must delete it.
    delete extension_host_;
    extension_host_ = nullptr;
  }
}

void CefBrowserPlatformDelegateAlloy::OnExtensionHostDeleted() {
  DCHECK(is_background_host_);
  DCHECK(extension_host_);
  extension_host_ = nullptr;
}
