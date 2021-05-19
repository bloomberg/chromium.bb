// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
#pragma once

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/chrome/browser_delegate.h"

#include "base/optional.h"

class CefBrowserContentsDelegate;
class CefRequestContextImpl;
class ChromeBrowserHostImpl;

// Implementation of the cef::BrowserDelegate interface. Lifespan is controlled
// by the Browser object. Only accessed on the UI thread.
//
// The Browser object represents the top-level Chrome browser window. One or
// more tabs (WebContents) are then owned by the Browser object via
// TabStripModel. A new Browser object can be created programmatically using
// "new Browser" or Browser::Create, or as a result of user action such as
// dragging a tab out of an existing window. New or existing tabs can also be
// added to an already existing Browser object.
//
// The Browser object acts as the WebContentsDelegate for all attached tabs. CEF
// integration requires WebContentsDelegate callbacks and notification of tab
// attach/detach. To support this integration a cef::BrowserDelegate
// (ChromeBrowserDelegate) member is created in the Browser constructor and
// receives delegation for the Browser callbacks. ChromeBrowserDelegate creates
// a new ChromeBrowserHostImpl when a tab is added to a Browser for the first
// time, and that ChromeBrowserHostImpl continues to exist until the tab's
// WebContents is destroyed. The associated WebContents object does not change,
// but the Browser object will change when the tab is dragged between windows.
class ChromeBrowserDelegate : public cef::BrowserDelegate {
 public:
  ChromeBrowserDelegate(Browser* browser,
                        const CefBrowserCreateParams& create_params);
  ~ChromeBrowserDelegate() override;

  // cef::BrowserDelegate methods:
  void OnWebContentsCreated(content::WebContents* new_contents) override;
  void SetAsDelegate(content::WebContents* web_contents,
                     bool set_delegate) override;

  // WebContentsDelegate methods:
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void AddNewContents(content::WebContents* source_contents,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  Browser* browser() const { return browser_; }

 private:
  void CreateBrowser(
      content::WebContents* web_contents,
      CefBrowserSettings settings,
      CefRefPtr<CefClient> client,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<ChromeBrowserHostImpl> opener,
      CefRefPtr<CefRequestContextImpl> request_context_impl);

  CefBrowserContentsDelegate* GetDelegateForWebContents(
      content::WebContents* web_contents);

  Browser* const browser_;

  // Used when creating a new browser host.
  const CefBrowserCreateParams create_params_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
