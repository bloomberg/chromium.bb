// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/shared_password_controller.h"

#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#include "components/password_manager/ios/test_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::Return;

namespace password_manager {

class MockPasswordManager : public PasswordManagerInterface {
 public:
  MOCK_METHOD(void, DidNavigateMainFrame, (bool), (override));
  MOCK_METHOD(void,
              OnPasswordFormsParsed,
              (PasswordManagerDriver*, const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormsRendered,
              (PasswordManagerDriver*,
               const std::vector<autofill::FormData>&,
               bool),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmitted,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmittedNoChecksForiOS,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              PresaveGeneratedPassword,
              (PasswordManagerDriver*,
               const autofill::FormData&,
               const base::string16&,
               autofill::FieldRendererId),
              (override));
  MOCK_METHOD(void,
              UpdateStateOnUserInput,
              (PasswordManagerDriver*,
               autofill::FormRendererId,
               autofill::FieldRendererId,
               const base::string16&),
              (override));
  MOCK_METHOD(void,
              OnPasswordNoLongerGenerated,
              (PasswordManagerDriver*),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormRemoved,
              (PasswordManagerDriver*,
               const autofill::FieldDataManager*,
               autofill::FormRendererId),
              (override));
  MOCK_METHOD(void,
              OnIframeDetach,
              (const std::string&,
               PasswordManagerDriver*,
               const autofill::FieldDataManager*),
              (override));
};

class SharedPasswordControllerTest : public PlatformTest {
 public:
  SharedPasswordControllerTest() : PlatformTest() {
    delegate_ = OCMProtocolMock(@protocol(SharedPasswordControllerDelegate));
    password_manager::PasswordManagerClient* client_ptr =
        &password_manager_client_;
    password_manager::PasswordManagerDriver* driver_ptr =
        &password_manager_driver_;
    [[[delegate_ stub] andReturnValue:OCMOCK_VALUE(client_ptr)]
        passwordManagerClient];
    [[[delegate_ stub] andReturnValue:OCMOCK_VALUE(driver_ptr)]
        passwordManagerDriver];
    form_helper_ = OCMStrictClassMock([PasswordFormHelper class]);
    suggestion_helper_ = OCMStrictClassMock([PasswordSuggestionHelper class]);
    OCMExpect([form_helper_ setDelegate:[OCMArg any]]);
    OCMExpect([suggestion_helper_ setDelegate:[OCMArg any]]);
    controller_ =
        [[SharedPasswordController alloc] initWithWebState:&web_state_
                                                   manager:&password_manager_
                                                formHelper:form_helper_
                                          suggestionHelper:suggestion_helper_];
    controller_.delegate = delegate_;
    [suggestion_helper_ verify];
    [form_helper_ verify];
    UniqueIDDataTabHelper::CreateForWebState(&web_state_);
  }

 protected:
  web::TestWebState web_state_;
  testing::StrictMock<MockPasswordManager> password_manager_;
  id form_helper_;
  id suggestion_helper_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  password_manager::StubPasswordManagerDriver password_manager_driver_;
  id delegate_;
  SharedPasswordController* controller_;
};

// Test that PasswordManager is notified of main frame navigation.
TEST_F(SharedPasswordControllerTest,
       PasswordManagerDidNavigationMainFrameOnNavigationFinished) {
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  navigation_context.SetIsSameDocument(false);
  navigation_context.SetIsRendererInitiated(true);

  EXPECT_CALL(password_manager_, DidNavigateMainFrame(true));
  web_state_.OnNavigationFinished(&navigation_context);
}

// Tests that forms are found, parsed, and sent to PasswordManager.
TEST_F(SharedPasswordControllerTest, FormsArePropagatedOnHTMLPageLoad) {
  web_state_.SetCurrentURL(GURL("https://www.chromium.org/"));
  web_state_.SetContentIsHTML(true);

  OCMExpect([suggestion_helper_ resetForNewPage]);

  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(
          const std::vector<autofill::FormData>& forms, uint32_t maxID)) {
        EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
        EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
        OCMExpect([suggestion_helper_ updateStateOnPasswordFormExtracted]);
        autofill::FormData form_data = test_helpers::MakeSimpleFormData();
        completionHandler({form_data}, 1);
        return YES;
      }];
  OCMExpect([form_helper_
      findPasswordFormsWithCompletionHandler:mock_completion_handler]);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests form finding and parsing is not triggered for non HTML pages.
TEST_F(SharedPasswordControllerTest, NoFormsArePropagatedOnNonHTMLPageLoad) {
  web_state_.SetCurrentURL(GURL("https://www.chromium.org/"));
  web_state_.SetContentIsHTML(false);

  OCMExpect([suggestion_helper_ resetForNewPage]);

  [[form_helper_ reject] findPasswordFormsWithCompletionHandler:[OCMArg any]];
  OCMExpect([suggestion_helper_ processWithNoSavedCredentials]);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests that new frames will trigger PasswordFormHelper to set up unique IDs.
TEST_F(SharedPasswordControllerTest, FormHelperSetsUpUniqueIDsForNewFrame) {
  [[[form_helper_ expect] ignoringNonObjectArgs]
      setUpForUniqueIDsWithInitialState:1
                                inFrame:static_cast<web::WebFrame*>(
                                            [OCMArg anyPointer])];
  web::FakeWebFrame web_frame("dummy-frame-id", /*is_main_frame=*/true, GURL());
  web_state_.OnWebFrameDidBecomeAvailable(&web_frame);
}

// Tests that suggestions are reported as unavailable for nonpassword forms.
TEST_F(SharedPasswordControllerTest,
       CheckNoSuggestionsAreAvailableForNonPasswordForm) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:@"frame-id"];

  id mock_completion_handler =
      [OCMArg checkWithBlock:^BOOL(void (^completionHandler)(BOOL)) {
        // Ensure |suggestion_helper_| reports no suggestions.
        completionHandler(NO);
        return YES;
      }];
  [[suggestion_helper_ expect]
      checkIfSuggestionsAvailableForForm:form_query
                             isMainFrame:YES
                                webState:&web_state_
                       completionHandler:mock_completion_handler];
  EXPECT_CALL(password_manager_,
              UpdateStateOnUserInput(_, form_query.uniqueFormID,
                                     form_query.uniqueFieldID, _));

  __block BOOL completion_was_called = NO;
  [controller_ checkIfSuggestionsAvailableForForm:form_query
                                      isMainFrame:YES
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  EXPECT_FALSE(suggestionsAvailable);
                                  completion_was_called = YES;
                                }];
  EXPECT_TRUE(completion_was_called);

  [suggestion_helper_ verify];
}

// Tests that no suggestions are returned if PasswordSuggestionHelper has none.
TEST_F(SharedPasswordControllerTest, ReturnsNoSuggestionsIfNoneAreAvailable) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:@"frame-id"];
  [[[suggestion_helper_ expect] andReturn:@[]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                          fieldType:form_query.fieldType];

  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 EXPECT_EQ(0UL, suggestions.count);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that suggestions are returned if PasswordSuggestionHelper has some.
TEST_F(SharedPasswordControllerTest, ReturnsSuggestionsIfAvailable) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:@"frame-id"];
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:@"value"
                       displayDescription:@"display-description"
                                     icon:@"icon"
                               identifier:0
                           requiresReauth:NO];
  [[[suggestion_helper_ expect] andReturn:@[ suggestion ]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                          fieldType:form_query.fieldType];

  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 EXPECT_EQ(1UL, suggestions.count);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that the "Suggest a password" suggestion is returned if the form is
// eligible for generation.
TEST_F(SharedPasswordControllerTest,
       ReturnsGenerateSuggestionIfFormIsEligibleForGeneration) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:@"frame-id"];
  [[[suggestion_helper_ expect] andReturn:@[]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                          fieldType:form_query.fieldType];
  EXPECT_CALL(*password_manager_client_.GetPasswordFeatureManager(),
              IsGenerationEnabled)
      .WillOnce(Return(true));

  autofill::PasswordFormGenerationData form_generation_data = {
      form_query.uniqueFormID, form_query.uniqueFieldID,
      form_query.uniqueFieldID};
  [controller_ formEligibleForGenerationFound:form_generation_data];
  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 ASSERT_EQ(1UL, suggestions.count);
                 FormSuggestion* suggestion = suggestions.firstObject;
                 EXPECT_EQ(autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY,
                           suggestion.identifier);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that accepting a "Suggest a password" suggestion will give a suggested
// password to the delegate.
TEST_F(SharedPasswordControllerTest, SuggestsGeneratedPassword) {
  autofill::FormRendererId form_id(0);
  autofill::FieldRendererId field_id(1);
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, field_id, field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
               identifier:autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY
           requiresReauth:NO];

  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:[OCMArg any]];

  [controller_ didSelectSuggestion:suggestion
                              form:@"test-form-name"
                      uniqueFormID:form_id
                   fieldIdentifier:@"test-field-id"
                     uniqueFieldID:field_id
                           frameID:@"test-frame-id"
                 completionHandler:nil];

  [delegate_ verify];
}

// Tests that generated passwords are presaved.
TEST_F(SharedPasswordControllerTest, PresavesGeneratedPassword) {
  autofill::FormRendererId form_id(0);
  autofill::FieldRendererId field_id(1);
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, field_id, field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
               identifier:autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY
           requiresReauth:NO];

  id decision_handler_arg =
      [OCMArg checkWithBlock:^(void (^decision_handler)(BOOL)) {
        decision_handler(/*accept=*/YES);
        return YES;
      }];
  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:decision_handler_arg];

  id fill_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL)) {
        completion_handler(/*success=*/YES);
        return YES;
      }];
  [[form_helper_ expect] fillPasswordForm:form_id
                    newPasswordIdentifier:field_id
                confirmPasswordIdentifier:field_id
                        generatedPassword:[OCMArg isNotNil]
                        completionHandler:fill_completion_handler_arg];

  autofill::FormData form_data = test_helpers::MakeSimpleFormData();
  id extract_completion_handler_arg = [OCMArg
      checkWithBlock:^(void (^completion_handler)(BOOL, autofill::FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:form_id
            completionHandler:extract_completion_handler_arg];

  EXPECT_CALL(password_manager_, PresaveGeneratedPassword);

  [controller_ didSelectSuggestion:suggestion
                              form:@"test-form-name"
                      uniqueFormID:form_id
                   fieldIdentifier:@"test-field-id"
                     uniqueFieldID:field_id
                           frameID:@"test-frame-id"
                 completionHandler:nil];

  [delegate_ verify];
}

// TODO(crbug.com/1097353): Finish unit testing the rest of the public API.

}  // namespace password_manager
