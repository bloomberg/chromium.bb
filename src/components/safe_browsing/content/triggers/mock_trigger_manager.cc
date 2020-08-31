// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/triggers/mock_trigger_manager.h"

namespace safe_browsing {

MockTriggerManager::MockTriggerManager()
    : TriggerManager(nullptr, nullptr, nullptr) {}

MockTriggerManager::~MockTriggerManager() {}

}  // namespace safe_browsing