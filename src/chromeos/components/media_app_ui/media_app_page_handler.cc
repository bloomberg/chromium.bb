// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_page_handler.h"

#include <utility>

#include "chromeos/components/media_app_ui/media_app_ui.h"
#include "chromeos/components/media_app_ui/media_app_ui_delegate.h"

MediaAppPageHandler::MediaAppPageHandler(
    chromeos::MediaAppUI* media_app_ui,
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), media_app_ui_(media_app_ui) {}

MediaAppPageHandler::~MediaAppPageHandler() = default;

void MediaAppPageHandler::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  auto error_message = media_app_ui_->delegate()->OpenFeedbackDialog();
  std::move(callback).Run(std::move(error_message));
}
