// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNC_ERROR_FACTORY_H_
#define SYNC_API_SYNC_ERROR_FACTORY_H_
#pragma once

#include <string>

#include "base/location.h"
#include "sync/api/sync_error.h"

class SyncErrorFactory {
 public:
  SyncErrorFactory();
  virtual ~SyncErrorFactory();

  // Creates a SyncError object and uploads this call stack to breakpad.
  virtual SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) = 0;
};

#endif  // SYNC_API_SYNC_ERROR_FACTORY_H_

