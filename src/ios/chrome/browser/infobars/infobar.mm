// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar.h"

#include <utility>

#include "base/logging.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfoBarIOS::InfoBarIOS(InfoBarController* controller,
                       std::unique_ptr<InfoBarDelegate> delegate)
    : InfoBar(std::move(delegate)), controller_(controller) {
  DCHECK(controller_);
  [controller_ setDelegate:this];
}

InfoBarIOS::~InfoBarIOS() {
  DCHECK(controller_);
  [controller_ detachView];
  controller_ = nil;
}

UIView* InfoBarIOS::view() {
  DCHECK(controller_);
  return [controller_ view];
}

void InfoBarIOS::RemoveView() {
  DCHECK(controller_);
  [controller_ removeView];
}

#pragma mark - InfoBarControllerDelegate

bool InfoBarIOS::IsOwned() {
  return owner() != nullptr;
}

void InfoBarIOS::RemoveInfoBar() {
  RemoveSelf();
}
