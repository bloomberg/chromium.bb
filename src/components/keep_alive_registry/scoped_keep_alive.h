// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_
#define COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_

#include "base/macros.h"

enum class KeepAliveOrigin;
enum class KeepAliveRestartOption;

// Registers with KeepAliveRegistry on creation and unregisters them
// on destruction. Use these objects with a scoped_ptr for easy management.
// Note: The registration will hit a CHECK if it happens while we are
// shutting down. Caller code should make sure that this can't happen.
class ScopedKeepAlive {
 public:
  ScopedKeepAlive(KeepAliveOrigin origin, KeepAliveRestartOption restart);
  ~ScopedKeepAlive();

 private:
  const KeepAliveOrigin origin_;
  const KeepAliveRestartOption restart_;

  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
};

#endif  // COMPONENTS_KEEP_ALIVE_REGISTRY_SCOPED_KEEP_ALIVE_H_
