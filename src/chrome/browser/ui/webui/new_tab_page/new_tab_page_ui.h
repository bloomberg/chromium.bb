// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include "base/macros.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class NavigationHandle;
class WebContents;
class WebUI;
}
class GURL;
class InstantService;
class NewTabPageHandler;
class Profile;

class NewTabPageUI : public ui::MojoWebUIController,
                     public new_tab_page::mojom::PageHandlerFactory,
                     public InstantServiceObserver,
                     content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);
  ~NewTabPageUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
          pending_receiver);

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler) override;

  // InstantServiceObserver:
  void NtpThemeChanged(const NtpTheme& theme) override;
  void MostVisitedInfoChanged(const InstantMostVisitedInfo& info) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Updates the load time data with the current theme's background color. That
  // way the background color is available as soon as the page loads and we
  // prevent a potential white flicker.
  void UpdateBackgroundColor(const NtpTheme& theme);

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_;
  Profile* profile_;
  InstantService* instant_service_;
  content::WebContents* web_contents_;
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(NewTabPageUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
