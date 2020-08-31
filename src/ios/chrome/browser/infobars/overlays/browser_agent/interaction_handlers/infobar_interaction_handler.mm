// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

#include "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarInteractionHandler::InfobarInteractionHandler(
    InfobarType infobar_type,
    std::unique_ptr<Handler> banner_handler,
    std::unique_ptr<Handler> detail_sheet_handler,
    std::unique_ptr<Handler> modal_handler)
    : infobar_type_(infobar_type),
      banner_handler_(std::move(banner_handler)),
      detail_sheet_handler_(std::move(detail_sheet_handler)),
      modal_handler_(std::move(modal_handler)) {
  DCHECK(banner_handler_);
}

InfobarInteractionHandler::~InfobarInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarInteractionHandler::CreateBannerCallbackInstaller() {
  return banner_handler_->CreateInstaller();
}

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarInteractionHandler::CreateDetailSheetCallbackInstaller() {
  return detail_sheet_handler_ ? detail_sheet_handler_->CreateInstaller()
                               : nullptr;
}

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarInteractionHandler::CreateModalCallbackInstaller() {
  return modal_handler_ ? modal_handler_->CreateInstaller() : nullptr;
}

void InfobarInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type,
    bool visible) {
  Handler* handler = nullptr;
  switch (overlay_type) {
    case InfobarOverlayType::kBanner:
      handler = banner_handler_.get();
      break;
    case InfobarOverlayType::kDetailSheet:
      handler = detail_sheet_handler_.get();
      break;
    case InfobarOverlayType::kModal:
      handler = modal_handler_.get();
      break;
  }
  if (handler)
    handler->InfobarVisibilityChanged(infobar, visible);
}
