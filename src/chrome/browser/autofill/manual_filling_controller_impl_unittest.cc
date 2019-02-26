// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::StrictMock;

constexpr char kExampleSite[] = "https://example.com";

class MockPasswordAccessoryController : public PasswordAccessoryController {
 public:
  MOCK_METHOD2(
      SavePasswordsForOrigin,
      void(const std::map<base::string16, const autofill::PasswordForm*>&,
           const url::Origin&));
  MOCK_METHOD3(
      OnAutomaticGenerationStatusChanged,
      void(bool,
           const base::Optional<
               autofill::password_generation::PasswordGenerationUIData>&,
           const base::WeakPtr<password_manager::PasswordManagerDriver>&));
  MOCK_METHOD1(OnFilledIntoFocusedField, void(autofill::FillingStatus));
  MOCK_METHOD3(RefreshSuggestionsForField,
               void(const url::Origin&, bool, bool));
  MOCK_METHOD0(DidNavigateMainFrame, void());
  MOCK_METHOD0(ShowWhenKeyboardIsVisible, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD2(GetFavicon,
               void(int, base::OnceCallback<void(const gfx::Image&)>));
  MOCK_METHOD2(OnFillingTriggered, void(bool, const base::string16&));
  MOCK_CONST_METHOD1(OnOptionSelected, void(const base::string16&));
  MOCK_METHOD0(OnGenerationRequested, void());
  MOCK_METHOD1(GeneratedPasswordAccepted, void(const base::string16&));
  MOCK_METHOD0(GeneratedPasswordRejected, void());
  MOCK_CONST_METHOD0(native_window, gfx::NativeWindow());
};

// The mock view mocks the platform-specific implementation. That also means
// that we have to care about the lifespan of the Controller because that would
// usually be responsibility of the view.
class MockPasswordAccessoryView : public ManualFillingViewInterface {
 public:
  MockPasswordAccessoryView() = default;

  MOCK_METHOD1(OnItemsAvailable, void(const autofill::AccessorySheetData&));
  MOCK_METHOD1(OnFillingTriggered, void(const base::string16&));
  MOCK_METHOD0(OnViewDestroyed, void());
  MOCK_METHOD1(OnAutomaticGenerationStatusChanged, void(bool));
  MOCK_METHOD0(CloseAccessorySheet, void());
  MOCK_METHOD0(SwapSheetWithKeyboard, void());
  MOCK_METHOD0(ShowWhenKeyboardIsVisible, void());
  MOCK_METHOD0(Hide, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordAccessoryView);
};

autofill::AccessorySheetData dummy_accessory_sheet_data() {
  constexpr char kExampleAccessorySheetDataTitle[] = "Example title";
  return autofill::AccessorySheetData(
      base::ASCIIToUTF16(kExampleAccessorySheetDataTitle));
}

}  // namespace

class ManualFillingControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManualFillingControllerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        std::make_unique<StrictMock<MockPasswordAccessoryView>>());
    NavigateAndCommit(GURL(kExampleSite));
  }

  ManualFillingControllerImpl* controller() {
    return ManualFillingControllerImpl::FromWebContents(web_contents());
  }

  MockPasswordAccessoryView* view() {
    return static_cast<MockPasswordAccessoryView*>(controller()->view());
  }

 protected:
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
};

TEST_F(ManualFillingControllerTest, IsNotRecreatedForSameWebContents) {
  ManualFillingController* initial_controller =
      ManualFillingControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  ManualFillingControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(ManualFillingControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest, ClosesViewWhenRefreshingSuggestions) {
  // Ignore Items - only the closing calls are interesting here.
  EXPECT_CALL(*view(), OnItemsAvailable(_)).Times(AnyNumber());

  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard())
      .Times(0);  // Don't touch the keyboard!
  controller()->RefreshSuggestionsForField(
      /*is_fillable=*/false, dummy_accessory_sheet_data());
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest,
       SwapSheetWithKeyboardWhenRefreshingSuggestions) {
  // Ignore Items - only the closing calls are interesting here.
  EXPECT_CALL(*view(), OnItemsAvailable(_)).Times(AnyNumber());

  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->RefreshSuggestionsForField(
      /*is_fillable=*/true, dummy_accessory_sheet_data());
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest, ClosesViewOnSuccessfullFillingOnly) {
  // If the filling wasn't successful, no call is expected.
  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->OnFilledIntoFocusedField(
      autofill::FillingStatus::ERROR_NOT_ALLOWED);
  controller()->OnFilledIntoFocusedField(
      autofill::FillingStatus::ERROR_NO_VALID_FIELD);

  // If the filling completed successfully, let the view know.
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->OnFilledIntoFocusedField(autofill::FillingStatus::SUCCESS);
}

TEST_F(ManualFillingControllerTest, RelaysShowAndHideKeyboardAccessory) {
  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->ShowWhenKeyboardIsVisible();
  EXPECT_CALL(*view(), Hide());
  controller()->Hide();
}

TEST_F(ManualFillingControllerTest, OnAutomaticGenerationStatusChanged) {
  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationStatusChanged(true);

  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(false));
  controller()->OnAutomaticGenerationStatusChanged(false);
}

TEST_F(ManualFillingControllerTest, OnFillingTriggered) {
  const char kTextToFill[] = "TextToFill";
  const base::string16 text_to_fill(base::ASCIIToUTF16(kTextToFill));

  EXPECT_CALL(mock_pwd_controller_, OnFillingTriggered(true, text_to_fill));
  controller()->OnFillingTriggered(true, text_to_fill);

  EXPECT_CALL(mock_pwd_controller_, OnFillingTriggered(false, text_to_fill));
  controller()->OnFillingTriggered(false, text_to_fill);
}

TEST_F(ManualFillingControllerTest, OnGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_, OnGenerationRequested());
  controller()->OnGenerationRequested();
}

TEST_F(ManualFillingControllerTest, GetFavicon) {
  constexpr int kIconSize = 75;
  auto icon_callback = base::BindOnce([](const gfx::Image&) {});

  EXPECT_CALL(mock_pwd_controller_, GetFavicon(kIconSize, _));
  controller()->GetFavicon(kIconSize, std::move(icon_callback));
}
