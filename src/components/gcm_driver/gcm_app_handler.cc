// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

GCMAppHandler::GCMAppHandler() {}
GCMAppHandler::~GCMAppHandler() {}

bool GCMAppHandler::CanHandle(const std::string& app_id) const {
  return false;
}

}  // namespace gcm
