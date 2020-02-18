// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_host.h"

class Browser;
class Profile;

namespace content {
class SiteInstance;
class WebContents;
}

namespace extensions {

class ExtensionView;

// The ExtensionHost for an extension that backs a view in the browser UI. For
// example, this could be an extension popup or dialog, but not a background
// page.
class ExtensionViewHost
    : public ExtensionHost,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public content::NotificationObserver {
 public:
  ExtensionViewHost(const Extension* extension,
                    content::SiteInstance* site_instance,
                    const GURL& url,
                    ViewType host_type);
  ~ExtensionViewHost() override;

  Browser* browser() { return browser_; }
  ExtensionView* view() { return view_.get(); }
  const ExtensionView* view() const { return view_.get(); }

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  void SetAssociatedWebContents(content::WebContents* web_contents);

  // Handles keyboard events that were not handled by HandleKeyboardEvent().
  // Platform specific implementation may override this method to handle the
  // event in platform specific way. Returns whether the events are handled.
  virtual bool UnhandledKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event);

  // ExtensionHost
  void OnDidStopFirstLoad() override;
  void LoadInitialURL() override;
  bool IsBackgroundPage() const override;

  // content::WebContentsDelegate
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool ShouldTransferNavigation(bool is_main_frame_navigation) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  // content::WebContentsObserver
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  // web_modal::WebContentsModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // extensions::ExtensionFunctionDispatcher::Delegate
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;
  content::WebContents* GetVisibleWebContents() const override;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Implemented per-platform. Create the platform-specific ExtensionView.
  static std::unique_ptr<ExtensionView> CreateExtensionView(
      ExtensionViewHost* host,
      Profile* profile);

  // The browser associated with the ExtensionView, if any.
  Browser* browser_ = nullptr;

  // Optional view that shows the rendered content in the UI.
  std::unique_ptr<ExtensionView> view_;

  // The relevant WebContents associated with this ExtensionViewHost, if any.
  content::WebContents* associated_web_contents_ = nullptr;

  // Observer to detect when the associated web contents is destroyed.
  class AssociatedWebContentsObserver;
  std::unique_ptr<AssociatedWebContentsObserver>
      associated_web_contents_observer_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewHost);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
