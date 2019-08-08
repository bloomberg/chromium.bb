// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/password_generation_controller_impl.h"

#include <map>
#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::password_generation::PasswordGenerationUIData;
using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;

  MOCK_METHOD0(GetPasswordGenerationHelper,
               password_manager::PasswordGenerationFrameHelper*());
  MOCK_METHOD1(GeneratedPasswordAccepted, void(const base::string16&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerDriver);
};

class MockPasswordGenerationHelper
    : public password_manager::PasswordGenerationFrameHelper {
 public:
  MockPasswordGenerationHelper(password_manager::PasswordManagerClient* client,
                               password_manager::PasswordManagerDriver* driver)
      : password_manager::PasswordGenerationFrameHelper(client, driver) {}

  MOCK_METHOD5(GeneratePassword,
               base::string16(const GURL&,
                              autofill::FormSignature,
                              autofill::FieldSignature,
                              uint32_t,
                              uint32_t*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationHelper);
};

// Mock modal dialog view used to bypass the need of a valid top level window.
class MockPasswordGenerationDialogView
    : public PasswordGenerationDialogViewInterface {
 public:
  MockPasswordGenerationDialogView() = default;

  MOCK_METHOD2(Show,
               void(base::string16&,
                    base::WeakPtr<password_manager::PasswordManagerDriver>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationDialogView);
};

PasswordGenerationUIData GetTestGenerationUIData1() {
  PasswordGenerationUIData data;

  PasswordForm& form = data.password_form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example1.com/accounts/Login");
  form.form_data.url = GURL("http://www.example1.com/accounts/LoginAuth");

  data.generation_element = ASCIIToUTF16("testelement1");
  data.max_length = 10;

  return data;
}

PasswordGenerationUIData GetTestGenerationUIData2() {
  PasswordGenerationUIData data;

  PasswordForm& form = data.password_form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example2.com/accounts/Login");
  form.form_data.url = GURL("http://www.example2.com/accounts/LoginAuth");

  data.generation_element = ASCIIToUTF16("testelement2");
  data.max_length = 10;

  return data;
}

MATCHER_P(PointsToSameAddress, expected, "") {
  return arg.get() == expected;
}

}  // namespace

class PasswordGenerationControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_manual_filling_controller_.AsWeakPtr(),
        mock_dialog_factory_.Get());

    mock_password_manager_driver_ =
        std::make_unique<NiceMock<MockPasswordManagerDriver>>();
    mock_generation_helper_ =
        std::make_unique<NiceMock<MockPasswordGenerationHelper>>(
            nullptr, mock_password_manager_driver_.get());
    mock_dialog_ =
        std::make_unique<NiceMock<MockPasswordGenerationDialogView>>();
  }

  PasswordGenerationController* controller() {
    return PasswordGenerationControllerImpl::FromWebContents(web_contents());
  }

  const base::MockCallback<
      PasswordGenerationControllerImpl::CreateDialogFactory>&
  mock_dialog_factory() {
    return mock_dialog_factory_;
  }

 protected:
  // Sets up mocks needed by the generation flow and signals the
  // |PasswordGenerationController| that generation is available.
  void InitializeGeneration(const base::string16& password);

  StrictMock<MockManualFillingController> mock_manual_filling_controller_;

  std::unique_ptr<NiceMock<MockPasswordManagerDriver>>
      mock_password_manager_driver_;
  std::unique_ptr<NiceMock<MockPasswordGenerationHelper>>
      mock_generation_helper_;
  std::unique_ptr<NiceMock<MockPasswordGenerationDialogView>> mock_dialog_;

 private:
  NiceMock<
      base::MockCallback<PasswordGenerationControllerImpl::CreateDialogFactory>>
      mock_dialog_factory_;
};

void PasswordGenerationControllerTest::InitializeGeneration(
    const base::string16& password) {
  ON_CALL(*mock_password_manager_driver_, GetPasswordGenerationHelper())
      .WillByDefault(Return(mock_generation_helper_.get()));

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));

  controller()->OnAutomaticGenerationAvailable(
      GetTestGenerationUIData1(), mock_password_manager_driver_->AsWeakPtr());

  ON_CALL(*mock_generation_helper_, GeneratePassword(_, _, _, _, _))
      .WillByDefault(Return(password));

  ON_CALL(mock_dialog_factory(), Run)
      .WillByDefault(Return(ByMove(std::move(mock_dialog_))));
}

TEST_F(PasswordGenerationControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordGenerationController* initial_controller =
      PasswordGenerationControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordGenerationControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(PasswordGenerationControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordGenerationControllerTest, RelaysAutomaticGenerationAvailable) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationAvailable(GetTestGenerationUIData1(),
                                               nullptr);
}

TEST_F(PasswordGenerationControllerTest, OnlySignalsGenerationUnavailableOnce) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationAvailable(
      GetTestGenerationUIData1(), mock_password_manager_driver_->AsWeakPtr());
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->OnGenerationElementLostFocus();
  controller()->OnGenerationElementLostFocus();
}

TEST_F(PasswordGenerationControllerTest,
       OnlySendsGenerationUnavailableIfAvailableBefore) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false))
      .Times(0);
  controller()->OnGenerationElementLostFocus();
}

// Tests that if AutomaticGenerationAvailable is called for different
// password forms, the form and field signatures used for password generation
// are updated.
TEST_F(PasswordGenerationControllerTest,
       UpdatesSignaturesForDifferentGenerationForms) {
  // Called twice for different forms.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true))
      .Times(2);
  controller()->OnAutomaticGenerationAvailable(
      GetTestGenerationUIData1(), mock_password_manager_driver_->AsWeakPtr());
  PasswordGenerationUIData new_ui_data = GetTestGenerationUIData2();
  controller()->OnAutomaticGenerationAvailable(
      new_ui_data, mock_password_manager_driver_->AsWeakPtr());

  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(new_ui_data.password_form.form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureByNameAndType(
          new_ui_data.generation_element, "password");

  MockPasswordGenerationDialogView* raw_dialog_view = mock_dialog_.get();

  base::string16 generated_password = ASCIIToUTF16("t3stp@ssw0rd");
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  EXPECT_CALL(*mock_password_manager_driver_, GetPasswordGenerationHelper())
      .WillOnce(Return(mock_generation_helper_.get()));
  EXPECT_CALL(*mock_generation_helper_,
              GeneratePassword(_, form_signature, field_signature,
                               uint32_t(new_ui_data.max_length), _))
      .WillOnce(Return(generated_password));
  EXPECT_CALL(*raw_dialog_view,
              Show(generated_password,
                   PointsToSameAddress(mock_password_manager_driver_.get())));
  controller()->OnGenerationRequested();
}

TEST_F(PasswordGenerationControllerTest, RecordsGeneratedPasswordRejected) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");

  InitializeGeneration(test_password);

  base::HistogramTester histogram_tester;

  controller()->OnGenerationRequested();
  controller()->GeneratedPasswordRejected();

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GeneratedPasswordDialog", false, 1);
}

TEST_F(PasswordGenerationControllerTest, RecordsGeneratedPasswordAccepted) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");

  InitializeGeneration(test_password);

  base::HistogramTester histogram_tester;

  controller()->OnGenerationRequested();
  controller()->GeneratedPasswordAccepted(
      test_password, mock_password_manager_driver_->AsWeakPtr());

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GeneratedPasswordDialog", true, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RelaysGenerationAcceptedToCorrectDriver) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");

  InitializeGeneration(test_password);

  controller()->OnGenerationRequested();

  MockPasswordManagerDriver wrong_driver;
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationAvailable(GetTestGenerationUIData2(),
                                               wrong_driver.AsWeakPtr());

  EXPECT_CALL(*mock_password_manager_driver_,
              GeneratedPasswordAccepted(test_password));
  EXPECT_CALL(wrong_driver, GeneratedPasswordAccepted(_)).Times(0);
  controller()->GeneratedPasswordAccepted(
      test_password, mock_password_manager_driver_->AsWeakPtr());
}
