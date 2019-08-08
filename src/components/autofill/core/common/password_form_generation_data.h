// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_

#include <stdint.h>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures_util.h"
#include "url/gurl.h"

namespace autofill {

// Structure used for sending information from browser to renderer about on
// which fields password should be generated.
struct PasswordFormGenerationData {
  PasswordFormGenerationData();
  PasswordFormGenerationData(FormSignature form_signature,
                             FieldSignature field_signature);
  PasswordFormGenerationData(const PasswordFormGenerationData& other);
  ~PasswordFormGenerationData();

  // The unique signature of form where password should be generated
  // (see components/autofill/core/browser/form_structure.h).
  FormSignature form_signature;

  // The unique signature of field where password should be generated
  // (see components/autofill/core/browser/autofill_field.h).
  FieldSignature field_signature;

  // The unique signature of the confirmation field where the generated password
  // should be copied to.
  base::Optional<FieldSignature> confirmation_field_signature;
};

// Structure used for sending information from browser to renderer about on
// which fields password should be generated.
// TODO(https://crbug.com/866444): Remove old PasswordFormGenerationData and
// rename to PasswordFormGenerationData when the old parser is gone.
struct NewPasswordFormGenerationData {
  NewPasswordFormGenerationData();
  NewPasswordFormGenerationData(const NewPasswordFormGenerationData& other);
  NewPasswordFormGenerationData(uint32_t new_password_renderer_id,
                                uint32_t confirmation_password_renderer_id);
#if defined(OS_IOS)
  NewPasswordFormGenerationData(base::string16 form_name,
                                base::string16 new_password_element,
                                base::string16 confirmation_password_element);

  base::string16 form_name;
  base::string16 new_password_element;
  base::string16 confirmation_password_element;
#endif
  uint32_t new_password_renderer_id =
      FormFieldData::kNotSetFormControlRendererId;
  uint32_t confirmation_password_renderer_id =
      FormFieldData::kNotSetFormControlRendererId;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
