// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/browser_url_rewriter_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserURLRewriterProvider::BrowserURLRewriterProvider() = default;

BrowserURLRewriterProvider::~BrowserURLRewriterProvider() = default;

void BrowserURLRewriterProvider::AddProviderRewriters(
    web::BrowserURLRewriter* rewriter) {}
