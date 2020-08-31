// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"

#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeInfobarIOS::FakeInfobarIOS(InfobarType type, base::string16 message_text)
    : InfoBarIOS(type, std::make_unique<FakeInfobarDelegate>(message_text)) {}

FakeInfobarIOS::FakeInfobarIOS(
    std::unique_ptr<FakeInfobarDelegate> fake_delegate)
    : InfoBarIOS(InfobarType::kInfobarTypeConfirm, std::move(fake_delegate)) {}

FakeInfobarIOS::~FakeInfobarIOS() = default;
