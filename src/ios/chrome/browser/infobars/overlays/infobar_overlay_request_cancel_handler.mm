// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"

#include "base/check.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarManager;

#pragma mark - InfobarOverlayRequestCancelHandler

InfobarOverlayRequestCancelHandler::InfobarOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    InfoBarIOS* infobar)
    : OverlayRequestCancelHandler(request, queue),
      infobar_(infobar),
      removal_observer_(this) {
  DCHECK(infobar_);
}

InfobarOverlayRequestCancelHandler::~InfobarOverlayRequestCancelHandler() =
    default;

#pragma mark - Protected

void InfobarOverlayRequestCancelHandler::HandleReplacement(
    InfoBarIOS* replacement) {}

#pragma mark - Private

void InfobarOverlayRequestCancelHandler::CancelForInfobarRemoval() {
  CancelRequest();
}

#pragma mark - InfobarOverlayRequestCancelHandler::RemovalObserver

InfobarOverlayRequestCancelHandler::RemovalObserver::RemovalObserver(
    InfobarOverlayRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler), scoped_observer_(this) {
  DCHECK(cancel_handler_);
  InfoBarManager* manager = cancel_handler_->infobar()->owner();
  DCHECK(manager);
  scoped_observer_.Add(manager);
}

InfobarOverlayRequestCancelHandler::RemovalObserver::~RemovalObserver() =
    default;

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (cancel_handler_->infobar() == infobar) {
    cancel_handler_->CancelForInfobarRemoval();
    // The cancel handler is destroyed after Cancel(), so no code can be added
    // after this call.
  }
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarReplaced(
    InfoBar* old_infobar,
    InfoBar* new_infobar) {
  if (cancel_handler_->infobar() == old_infobar) {
    cancel_handler_->HandleReplacement(static_cast<InfoBarIOS*>(new_infobar));
    cancel_handler_->CancelForInfobarRemoval();
    // The cancel handler is destroyed after Cancel(), so no code can be added
    // after this call.
  }
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  scoped_observer_.Remove(manager);
  cancel_handler_->CancelForInfobarRemoval();
  // The cancel handler is destroyed after Cancel(), so no code can be added
  // after this call.
}
