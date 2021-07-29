// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_MEMORIES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_MEMORIES_UI_H_

#include "chrome/browser/ui/webui/history_clusters/history_clusters.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class HistoryClustersHandler;

namespace content {
class WebContents;
class WebUI;
}  // namespace content

// The UI for chrome://memories/
class MemoriesUI : public ui::MojoWebUIController {
 public:
  explicit MemoriesUI(content::WebUI* contents);
  ~MemoriesUI() override;

  MemoriesUI(const MemoriesUI&) = delete;
  MemoriesUI& operator=(const MemoriesUI&) = delete;

  // Instantiates the implementor of the history_clusters::mojom::PageHandler
  // mojo interface passing to it the pending receiver that will be internally
  // bound.
  void BindInterface(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                         pending_page_handler);

 private:
  Profile* profile_;
  content::WebContents* web_contents_;
  std::unique_ptr<HistoryClustersHandler> history_clusters_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_MEMORIES_UI_H_
