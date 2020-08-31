// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_
#define WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_

#include "base/callback_forward.h"
#include "components/autofill/core/browser/test_autofill_provider.h"

namespace weblayer {

// A stub AutofillProvider implementation that is used in cross-platform
// integration tests of renderer-side autofill detection and communication to
// the browser.
class StubAutofillProvider : public autofill::TestAutofillProvider {
 public:
  explicit StubAutofillProvider(
      const base::RepeatingCallback<void(const autofill::FormData&)>&
          on_received_form_data);
  ~StubAutofillProvider() override;

  // AutofillProvider:
  void OnQueryFormFieldAutofill(
      autofill::AutofillHandlerProxy* handler,
      int32_t id,
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const gfx::RectF& bounding_box,
      bool /*unused_autoselect_first_suggestion*/) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubAutofillProvider);

  base::RepeatingCallback<void(const autofill::FormData&)>
      on_received_form_data_;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_STUB_AUTOFILL_PROVIDER_H_
