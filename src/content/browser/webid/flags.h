// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FLAGS_H_
#define CONTENT_BROWSER_WEBID_FLAGS_H_

// Flags to control WebID for testing/debugging.

namespace content {

// Whether the FedCM JavaScript API is enabled.
bool IsFedCmEnabled();

// Whether FedCM HTTP filtering is enabled.
bool IsFedCmInterceptionEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FLAGS_H_
