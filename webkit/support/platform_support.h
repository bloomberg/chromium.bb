// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_PLATFORM_SUPORT_H_
#define WEBKIT_SUPPORT_PLATFORM_SUPORT_H_

namespace webkit_support {
// Called before WebKit::initialize().
void BeforeInitialize();

// Called after WebKit::initialize().
void AfterInitialize();

// Called before WebKit::shutdown().
void BeforeShutdown();

// Called after WebKit::shutdown().
void AfterShutdown();
}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_PLATFORM_SUPORT_H_
