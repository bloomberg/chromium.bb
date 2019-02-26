// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_service_test_helper.h"

#include "services/ws/window_service.h"

namespace ws {

WindowServiceTestHelper::WindowServiceTestHelper(WindowService* window_service)
    : window_service_(window_service) {}

WindowServiceTestHelper::~WindowServiceTestHelper() = default;

}  // namespace ws
