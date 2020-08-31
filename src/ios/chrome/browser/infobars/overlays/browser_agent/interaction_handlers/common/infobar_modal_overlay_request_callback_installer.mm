// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarModalOverlayRequestCallbackInstaller::
    InfobarModalOverlayRequestCallbackInstaller(
        const OverlayRequestSupport* request_support,
        InfobarModalInteractionHandler* interaction_handler)
    : request_support_(request_support),
      interaction_handler_(interaction_handler) {
  DCHECK(request_support_);
  DCHECK(interaction_handler_);
}

InfobarModalOverlayRequestCallbackInstaller::
    ~InfobarModalOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void InfobarModalOverlayRequestCallbackInstaller::MainActionCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->PerformMainAction(infobar);
}

#pragma mark - OverlayRequestCallbackInstaller

const OverlayRequestSupport*
InfobarModalOverlayRequestCallbackInstaller::GetRequestSupport() const {
  return request_support_;
}

void InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  request->GetCallbackManager()->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &InfobarModalOverlayRequestCallbackInstaller::MainActionCallback,
          weak_factory_.GetWeakPtr(), request),
      InfobarModalMainActionResponse::ResponseSupport()));
}
