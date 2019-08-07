// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_service.h"

#include <utility>

namespace ash {

TestAssistantService::TestAssistantService() : binding_(this) {}

TestAssistantService::~TestAssistantService() = default;

chromeos::assistant::mojom::AssistantPtr
TestAssistantService::CreateInterfacePtrAndBind() {
  chromeos::assistant::mojom::AssistantPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestAssistantService::CacheScreenContext(
    CacheScreenContextCallback callback) {
  std::move(callback).Run();
}
}  // namespace ash
