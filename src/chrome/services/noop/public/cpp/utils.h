// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_NOOP_PUBLIC_CPP_UTILS_H_
#define CHROME_SERVICES_NOOP_PUBLIC_CPP_UTILS_H_

namespace chrome {

// Make sure the no-op service is started if it is enabled.
void MaybeStartNoopService();

// Whether the no-op service should be launched.
bool IsNoopServiceEnabled();

}  // namespace chrome

#endif  // CHROME_SERVICES_NOOP_PUBLIC_CPP_UTILS_H_
