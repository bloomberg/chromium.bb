// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/navigate_params.h"

CefNavigateParams::CefNavigateParams(const GURL& a_url,
                                     ui::PageTransition a_transition)
    : url(a_url), transition(a_transition) {}

CefNavigateParams::~CefNavigateParams() {}
