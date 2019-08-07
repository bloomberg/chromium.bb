// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_

#include "chrome/browser/autofill/manual_filling_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockManualFillingController
    : public ManualFillingController,
      public base::SupportsWeakPtr<MockManualFillingController> {
 public:
  MockManualFillingController();
  ~MockManualFillingController() override;

  MOCK_METHOD1(OnAutomaticGenerationStatusChanged, void(bool));
  MOCK_METHOD1(OnFilledIntoFocusedField, void(autofill::FillingStatus));
  MOCK_METHOD2(RefreshSuggestionsForField,
               void(autofill::mojom::FocusedFieldType,
                    const autofill::AccessorySheetData&));
  MOCK_METHOD1(ShowWhenKeyboardIsVisible,
               void(ManualFillingController::FillingSource));
  MOCK_METHOD0(ShowTouchToFillSheet, void());
  MOCK_METHOD1(Hide, void(ManualFillingController::FillingSource));
  MOCK_METHOD2(GetFavicon,
               void(int, base::OnceCallback<void(const gfx::Image&)>));
  MOCK_METHOD2(OnFillingTriggered,
               void(autofill::AccessoryTabType type,
                    const autofill::UserInfo::Field&));
  MOCK_CONST_METHOD1(OnOptionSelected,
                     void(autofill::AccessoryAction selected_action));
  MOCK_METHOD0(OnGenerationRequested, void());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
