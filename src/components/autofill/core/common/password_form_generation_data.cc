// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_generation_data.h"

#include <utility>

namespace autofill {

PasswordFormGenerationData::PasswordFormGenerationData() = default;

PasswordFormGenerationData::PasswordFormGenerationData(
    FormSignature form_signature,
    FieldSignature field_signature)
    : form_signature(form_signature), field_signature(field_signature) {}

PasswordFormGenerationData::PasswordFormGenerationData(
    const PasswordFormGenerationData& other) = default;

PasswordFormGenerationData::~PasswordFormGenerationData() = default;

NewPasswordFormGenerationData::NewPasswordFormGenerationData() = default;

NewPasswordFormGenerationData::NewPasswordFormGenerationData(
    const NewPasswordFormGenerationData& other) = default;

NewPasswordFormGenerationData::NewPasswordFormGenerationData(
    uint32_t new_password_renderer_id,
    uint32_t confirmation_password_renderer_id)
    : new_password_renderer_id(new_password_renderer_id),
      confirmation_password_renderer_id(confirmation_password_renderer_id) {}

#if defined(OS_IOS)
NewPasswordFormGenerationData::NewPasswordFormGenerationData(
    base::string16 form_name,
    base::string16 new_password_element,
    base::string16 confirmation_password_element)
    : form_name(std::move(form_name)),
      new_password_element(std::move(new_password_element)),
      confirmation_password_element(std::move(confirmation_password_element)) {}
#endif

}  // namespace autofill
