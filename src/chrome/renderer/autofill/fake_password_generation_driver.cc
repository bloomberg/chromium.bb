// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/fake_password_generation_driver.h"

#include <utility>

FakePasswordGenerationDriver::FakePasswordGenerationDriver() : binding_(this) {}

FakePasswordGenerationDriver::~FakePasswordGenerationDriver() = default;

void FakePasswordGenerationDriver::BindRequest(
    autofill::mojom::PasswordGenerationDriverAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void FakePasswordGenerationDriver::Flush() {
  if (binding_.is_bound())
    binding_.FlushForTesting();
}

void FakePasswordGenerationDriver::GenerationAvailableForForm(
    const autofill::PasswordForm& form) {
  called_generation_available_for_form_ = true;
}

void FakePasswordGenerationDriver::PasswordGenerationRejectedByTyping() {
  called_password_generation_rejected_by_typing_ = true;
}
