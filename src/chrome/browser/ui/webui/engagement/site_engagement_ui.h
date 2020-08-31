// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_

#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_details.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

// The UI for chrome://site-engagement/.
class SiteEngagementUI : public ui::MojoWebUIController {
 public:
  explicit SiteEngagementUI(content::WebUI* web_ui);
  ~SiteEngagementUI() override;

  // Instantiates the implementor of the mojom::SiteEngagementDetailsProvider
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::SiteEngagementDetailsProvider> receiver);

 private:
  std::unique_ptr<mojom::SiteEngagementDetailsProvider> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_
