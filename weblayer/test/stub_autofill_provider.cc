// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/stub_autofill_provider.h"

namespace weblayer {

StubAutofillProvider::StubAutofillProvider(
    const base::RepeatingCallback<void(const autofill::FormData&)>&
        on_received_form_data)
    : on_received_form_data_(on_received_form_data) {}

StubAutofillProvider::~StubAutofillProvider() = default;

void StubAutofillProvider::OnQueryFormFieldAutofill(
    autofill::AutofillHandlerProxy* handler,
    int32_t id,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool /*unused_autoselect_first_suggestion*/) {
  on_received_form_data_.Run(form);
}

}  // namespace weblayer
