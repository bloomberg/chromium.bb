// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_

#include "ash/webui/media_app_ui/media_app_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class MediaAppUI;

// Implements the media_app mojom interface providing chrome://media-app
// with browser process functions to call from the renderer process.
class MediaAppPageHandler : public media_app_ui::mojom::PageHandler {
 public:
  MediaAppPageHandler(
      MediaAppUI* media_app_ui,
      mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver);
  ~MediaAppPageHandler() override;

  MediaAppPageHandler(const MediaAppPageHandler&) = delete;
  MediaAppPageHandler& operator=(const MediaAppPageHandler&) = delete;

  // media_app_ui::mojom::PageHandler:
  void OpenFeedbackDialog(OpenFeedbackDialogCallback callback) override;

 private:
  mojo::Receiver<media_app_ui::mojom::PageHandler> receiver_;
  MediaAppUI* media_app_ui_;  // Owns |this|.
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_
