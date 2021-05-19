// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/browser/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/shared_password_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormRendererId;
using autofill::FieldRendererId;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {

const char kApplicationLocale[] = "en-US";
NSString* const kTestFormName = @"FormName";
FormRendererId kTestUniqueFormID = FormRendererId(0);
NSString* const kTestFieldIdentifier = @"FieldIdentifier";
FieldRendererId kTestUniqueFieldID = FieldRendererId(1);
NSString* const kTestFieldValue = @"FieldValue";
NSString* const kTestDisplayDescription = @"DisplayDescription";

}  // namespace

class CWVAutofillControllerTest : public web::WebTest {
 protected:
  CWVAutofillControllerTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);

    web_state_.SetBrowserState(&browser_state_);

    UniqueIDDataTabHelper::CreateForWebState(&web_state_);

    autofill_agent_ =
        [[FakeAutofillAgent alloc] initWithPrefService:&pref_service_
                                              webState:&web_state_];

    frame_id_ = base::SysUTF8ToNSString(web::kMainFakeFrameId);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_.SetWebFramesManager(std::move(frames_manager));

    auto password_manager_client =
        std::make_unique<WebViewPasswordManagerClient>(
            &web_state_, /*sync_service=*/nullptr, &pref_service_,
            /*identity_manager=*/nullptr, /*log_manager=*/nullptr,
            /*profile_store=*/nullptr, /*account_store=*/nullptr,
            /*requirements_service=*/nullptr);
    auto password_manager = std::make_unique<password_manager::PasswordManager>(
        password_manager_client.get());
    auto password_manager_driver =
        std::make_unique<WebViewPasswordManagerDriver>(password_manager.get());
    password_controller_ = OCMClassMock([SharedPasswordController class]);
    password_manager_client_ = password_manager_client.get();

    auto autofill_client = std::make_unique<autofill::WebViewAutofillClientIOS>(
        kApplicationLocale, &pref_service_, &personal_data_manager_,
        /*autocomplete_history_manager=*/nullptr, &web_state_,
        /*identity_manager=*/nullptr, &strike_database_, &sync_service_,
        std::make_unique<autofill::StubLogManager>());
    autofill_controller_ = [[CWVAutofillController alloc]
             initWithWebState:&web_state_
               autofillClient:std::move(autofill_client)
                autofillAgent:autofill_agent_
              passwordManager:std::move(password_manager)
        passwordManagerClient:std::move(password_manager_client)
        passwordManagerDriver:std::move(password_manager_driver)
           passwordController:password_controller_
            applicationLocale:kApplicationLocale];
    form_activity_tab_helper_ =
        std::make_unique<autofill::TestFormActivityTabHelper>(&web_state_);
  }

  void SetUp() override {
    web::WebTest::SetUp();

    OverrideJavaScriptFeatures(
        {autofill::AutofillJavaScriptFeature::GetInstance()});
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    web::WebFrame* frame_ptr = frame.get();
    web_frames_manager_->AddWebFrame(std::move(frame));
    web_state_.OnWebFrameDidBecomeAvailable(frame_ptr);
  }

  TestingPrefServiceSimple pref_service_;
  web::FakeBrowserState browser_state_;
  web::FakeWebState web_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestStrikeDatabase strike_database_;
  syncer::TestSyncService sync_service_;
  NSString* frame_id_;
  web::FakeWebFramesManager* web_frames_manager_;
  CWVAutofillController* autofill_controller_;
  FakeAutofillAgent* autofill_agent_;
  id password_controller_;
  std::unique_ptr<autofill::TestFormActivityTabHelper>
      form_activity_tab_helper_;
  WebViewPasswordManagerClient* password_manager_client_;
};

// Tests CWVAutofillController fetch suggestions for profiles.
TEST_F(CWVAutofillControllerTest, FetchProfileSuggestions) {
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:kTestDisplayDescription
                                     icon:nil
                               identifier:0
                           requiresReauth:NO];
  [autofill_agent_ addSuggestion:suggestion
                     forFormName:kTestFormName
                 fieldIdentifier:kTestFieldIdentifier
                         frameID:frame_id_];

  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                             isMainFrame:NO
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(void (
                                             ^suggestionsAvailable)(BOOL)) {
                         suggestionsAvailable(NO);
                         return YES;
                       }]]);

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* suggestion = suggestions.firstObject;
    EXPECT_NSEQ(kTestFieldValue, suggestion.value);
    EXPECT_NSEQ(kTestDisplayDescription, suggestion.displayDescription);
    EXPECT_NSEQ(kTestFormName, suggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:frame_id_
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController fetch suggestions for passwords.
TEST_F(CWVAutofillControllerTest, FetchPasswordSuggestions) {
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0
                           requiresReauth:NO];
  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                             isMainFrame:NO
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(void (
                                             ^suggestionsAvailable)(BOOL)) {
                         suggestionsAvailable(YES);
                         return YES;
                       }]]);
  OCMExpect([password_controller_
      retrieveSuggestionsForForm:[OCMArg any]
                        webState:&web_state_
               completionHandler:[OCMArg checkWithBlock:^(void (
                                     ^completionHandler)(NSArray*, id)) {
                 completionHandler(@[ suggestion ], nil);
                 return YES;
               }]]);

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* suggestion = suggestions.firstObject;
    EXPECT_TRUE([suggestion isPasswordSuggestion]);
    EXPECT_NSEQ(kTestFieldValue, suggestion.value);
    EXPECT_NSEQ(kTestFormName, suggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:frame_id_
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController accepts suggestion.
TEST_F(CWVAutofillControllerTest, AcceptSuggestion) {
  FormSuggestion* form_suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0
                           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:form_suggestion
                                                   formName:kTestFormName
                                            fieldIdentifier:kTestFieldIdentifier
                                                    frameID:frame_id_
                                       isPasswordSuggestion:NO];
  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptSuggestion:suggestion
                       completionHandler:^{
                         accept_completion_was_called = YES;
                       }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return accept_completion_was_called;
  }));
  EXPECT_NSEQ(
      form_suggestion,
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_]);
}

// Tests CWVAutofillController delegate focus callback is invoked.
TEST_F(CWVAutofillControllerTest, FocusCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didFocusOnFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:frame_id_
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.unique_form_id = kTestUniqueFormID;
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.unique_field_id = kTestUniqueFieldID;
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = web::kMainFakeFrameId;
    params.has_user_gesture = true;
    params.type = "focus";
    auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
    form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
    [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked.
TEST_F(CWVAutofillControllerTest, InputCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didInputInFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:frame_id_
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = web::kMainFakeFrameId;
    params.type = "input";
    params.has_user_gesture = true;
    auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
    form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
    [delegate verify];
}

// Tests CWVAutofillController delegate blur callback is invoked.
TEST_F(CWVAutofillControllerTest, BlurCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
           didBlurOnFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:@""
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = "blur";
  params.has_user_gesture = true;
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);

  [delegate verify];
}

// Tests CWVAutofillController delegate submit callback is invoked.
TEST_F(CWVAutofillControllerTest, SubmitCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                                frameID:frame_id_
                          userInitiated:YES];
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(), base::SysNSStringToUTF8(kTestFormName),
      /*form_data=*/"",
      /*user_initiated=*/true,
      /*is_main_frame=*/true);

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                                frameID:frame_id_
                          userInitiated:NO];

  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(), base::SysNSStringToUTF8(kTestFormName),
      /*form_data=*/"",
      /*user_initiated=*/false,
      /*is_main_frame=*/true);

  [delegate verify];
}

// Tests that CWVAutofillController notifies user of password leaks.
TEST_F(CWVAutofillControllerTest, NotifyUserOfLeak) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  GURL leak_url("https://www.chromium.org");
  password_manager::CredentialLeakType leak_type =
      password_manager::CreateLeakType(password_manager::IsSaved(true),
                                       password_manager::IsReused(true),
                                       password_manager::IsSyncing(true));
  CWVPasswordLeakType expected_leak_type = CWVPasswordLeakTypeSaved |
                                           CWVPasswordLeakTypeUsedOnOtherSites |
                                           CWVPasswordLeakTypeSyncingNormally;
  OCMExpect([delegate autofillController:autofill_controller_
           notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(leak_url)
                                leakType:expected_leak_type]);

  password_manager_client_->NotifyUserCredentialsWereLeaked(
      leak_type, leak_url, base::SysNSStringToUTF16(@"fake-username"));

  [delegate verify];
}

// Tests that CWVAutofillController suggests passwords to its delegate.
TEST_F(CWVAutofillControllerTest, SuggestPasswordCallback) {
  NSString* fake_generated_password = @"12345";
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;
  OCMExpect([delegate autofillController:autofill_controller_
                suggestGeneratedPassword:fake_generated_password
                         decisionHandler:[OCMArg checkWithBlock:^(void (
                                             ^decisionHandler)(BOOL)) {
                           decisionHandler(/*accept=*/YES);
                           return YES;
                         }]]);
  __block BOOL decision_handler_called = NO;
  [autofill_controller_ sharedPasswordController:password_controller_
                  showGeneratedPotentialPassword:fake_generated_password
                                 decisionHandler:^(BOOL accept) {
                                   decision_handler_called = YES;
                                   EXPECT_TRUE(accept);
                                 }];
  EXPECT_TRUE(decision_handler_called);

  [delegate verify];
}

}  // namespace ios_web_view
