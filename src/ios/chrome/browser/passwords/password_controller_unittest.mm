// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/js_password_manager.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/shared_password_controller.h"
#include "components/password_manager/ios/test_helpers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_js_test.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCPartialMockObject.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormActivityParams;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using password_manager::PasswordForm;
using autofill::PasswordFormFillData;
using base::SysUTF8ToNSString;
using FillingAssistance =
    password_manager::PasswordFormMetricsRecorder::FillingAssistance;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormManager;
using password_manager::PasswordStoreConsumer;
using password_manager::prefs::kPasswordLeakDetectionEnabled;
using test_helpers::SetPasswordFormFillData;
using test_helpers::MakeSimpleFormData;
using testing::NiceMock;
using testing::Return;
using base::ASCIIToUTF16;
using base::SysUTF8ToNSString;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using testing::WithArg;
using testing::_;
using web::WebFrame;

namespace {

class MockWebState : public web::FakeWebState {
 public:
  MOCK_CONST_METHOD0(GetBrowserState, web::BrowserState*(void));
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override {
    std::move(callback).Run(false);
  }
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(password_manager::PasswordStore* store)
      : store_(store) {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(kPasswordLeakDetectionEnabled,
                                            true);
    safe_browsing::RegisterProfilePrefs(prefs_->registry());
  }

  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(IsIncognito, bool());
  MOCK_METHOD1(PromptUserToSaveOrUpdatePasswordPtr,
               void(PasswordFormManagerForUI*));
  MOCK_CONST_METHOD1(IsSavingAndFillingEnabled, bool(const GURL&));

  PrefService* GetPrefs() const override { return prefs_.get(); }

  password_manager::PasswordStore* GetProfilePasswordStore() const override {
    return store_;
  }

  network::mojom::NetworkContext* GetNetworkContext() const override {
    return &network_context_;
  }

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr(manager.release());
    return false;
  }

 private:
  mutable FakeNetworkContext network_context_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  password_manager::PasswordStore* const store_;
};

ACTION_P(SaveToScopedPtr, scoped) {
  scoped->reset(arg0);
}

// Creates PasswordController with the given |web_state| and a mock client
// using the given |store|. If not null, |weak_client| is filled with a
// non-owning pointer to the created client. The created controller is
// returned.
PasswordController* CreatePasswordController(
    web::WebState* web_state,
    password_manager::PasswordStore* store,
    MockPasswordManagerClient** weak_client) {
  auto client = std::make_unique<NiceMock<MockPasswordManagerClient>>(store);
  if (weak_client)
    *weak_client = client.get();
  return [[PasswordController alloc] initWithWebState:web_state
                                               client:std::move(client)];
}

PasswordForm CreatePasswordForm(const char* origin_url,
                                const char* username_value,
                                const char* password_value) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(origin_url);
  form.signon_realm = origin_url;
  form.username_value = ASCIIToUTF16(username_value);
  form.password_value = ASCIIToUTF16(password_value);
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  return form;
}

// Invokes the password store consumer with a single copy of |form|.
ACTION_P(InvokeConsumer, form) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.push_back(std::make_unique<PasswordForm>(form));
  arg0->OnGetPasswordStoreResults(std::move(result));
}

ACTION(InvokeEmptyConsumerWithForms) {
  arg0->OnGetPasswordStoreResults(std::vector<std::unique_ptr<PasswordForm>>());
}

}  // namespace

@interface PasswordFormHelper (Testing)

// Provides access to JavaScript Manager for testing with mocks.
@property(nonatomic) JsPasswordManager* jsPasswordManager;

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<PasswordForm>&))completionHandler;

@end

@interface JsPasswordManager (Testing)

// Provides access to JavaScript Manager for testing with mocks.
@property BOOL noFormsSeen;

@end

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface PasswordsTestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;

@end

@implementation PasswordsTestSuggestionController

@synthesize suggestions = _suggestions;

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
}

@end

// Fake JsPasswordManager that can be set to fail at filling to check
// that the fail is handled correctly.
@interface FakeJsPasswordManager : JsPasswordManager

- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:(void (^)(NSString*))completionHandler;

@property BOOL noFormsSeen;

- (instancetype)init;

@end

@implementation FakeJsPasswordManager

- (instancetype)init {
  self = [super init];
  if (self) {
    _noFormsSeen = YES;
  }
  return self;
}

- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  auto fakeCompletionHandler = ^(NSString* res) {
    _noFormsSeen = [res isEqualToString:@"[]"] ? YES : NO;
    completionHandler(res);
  };
  [super findPasswordFormsInFrame:frame
                completionHandler:fakeCompletionHandler];
}

@end

@interface SharedPasswordController (Testing)

// Provides access for testing.
@property(nonatomic, assign) BOOL isPasswordGenerated;

- (void)injectGeneratedPasswordForFormId:(FormRendererId)formIdentifier
                       generatedPassword:(NSString*)generatedPassword
                       completionHandler:(void (^)())completionHandler;

@end

class PasswordControllerTest : public ChromeWebTest {
 public:
  PasswordControllerTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()),
        store_(new testing::NiceMock<password_manager::MockPasswordStore>()) {}

  ~PasswordControllerTest() override { store_->ShutdownOnUIThread(); }

  void SetUp() override {
    ChromeWebTest::SetUp();
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable wating, since most tests have nothing to do with predictions. All
    // tests that test working with prediction should explicitly turn
    // predictions on.
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

    UniqueIDDataTabHelper::CreateForWebState(web_state());

    passwordController_ =
        CreatePasswordController(web_state(), store_.get(), &weak_client_);

    ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
        .WillByDefault(Return(true));

    @autoreleasepool {
      // Make sure the temporary array is released after SetUp finishes,
      // otherwise [passwordController_ suggestionProvider] will be retained
      // until PlatformTest teardown, at which point all Chrome objects are
      // already gone and teardown may access invalid memory.
      suggestionController_ = [[PasswordsTestSuggestionController alloc]
          initWithWebState:web_state()
                 providers:@[ [passwordController_ suggestionProvider] ]];
      accessoryMediator_ =
          [[FormInputAccessoryMediator alloc] initWithConsumer:nil
                                                      delegate:nil
                                                  webStateList:nullptr
                                           personalDataManager:nullptr
                                                 passwordStore:nullptr
                                                      appState:nil
                                          securityAlertHandler:nil
                                        reauthenticationModule:nil];
      [accessoryMediator_ injectWebState:web_state()];
      [accessoryMediator_ injectProvider:suggestionController_];
    }
  }

  void WaitForFormManagersCreation() {
    auto& form_managers = passwordController_.passwordManager->form_managers();
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return !form_managers.empty();
    }));
  }

  void SimulateFormActivityObserverSignal(std::string type,
                                          FormRendererId form_id,
                                          FieldRendererId field_id,
                                          std::string value) {
    std::string mainFrameID = web::GetMainWebFrameId(web_state());
    WebFrame* frame = web::GetWebFrameWithId(web_state(), mainFrameID);
    FormActivityParams params;
    params.type = type;
    params.unique_form_id = form_id;
    params.frame_id = mainFrameID;
    params.value = value;
    [passwordController_.sharedPasswordController webState:web_state()
                                   didRegisterFormActivity:params
                                                   inFrame:frame];
  }

 protected:
  // Helper method for PasswordControllerTest.DontFillReadonly. Tries to load
  // |html| and find and fill there a form with hard-coded form data. Returns
  // YES on success, NO otherwise.
  BOOL BasicFormFill(NSString* html);

  // Retrieve the current suggestions from suggestionController_.
  NSArray* GetSuggestionValues() {
    NSMutableArray* suggestion_values = [NSMutableArray array];
    for (FormSuggestion* suggestion in [suggestionController_ suggestions])
      [suggestion_values addObject:suggestion.value];
    return [suggestion_values copy];
  }

  // Returns an identifier for the |form_number|th form in the page.
  std::string FormName(int form_number) {
    NSString* kFormNamingScript =
        @"__gCrWeb.form.getFormIdentifier("
         "    document.querySelectorAll('form')[%d]);";
    return base::SysNSStringToUTF8(ExecuteJavaScript(
        [NSString stringWithFormat:kFormNamingScript, form_number]));
  }

  void SimulateUserTyping(const std::string& form_name,
                          FormRendererId uniqueFormID,
                          const std::string& field_identifier,
                          FieldRendererId uniqueFieldID,
                          const std::string& typed_value,
                          const std::string& main_frame_id) {
    __block BOOL completion_handler_called = NO;
    FormSuggestionProviderQuery* form_query =
        [[FormSuggestionProviderQuery alloc]
            initWithFormName:SysUTF8ToNSString(form_name)
                uniqueFormID:uniqueFormID
             fieldIdentifier:SysUTF8ToNSString(field_identifier)
               uniqueFieldID:uniqueFieldID
                   fieldType:@"not_important"
                        type:@"input"
                  typedValue:SysUTF8ToNSString(typed_value)
                     frameID:SysUTF8ToNSString(main_frame_id)];
    [passwordController_.sharedPasswordController
        checkIfSuggestionsAvailableForForm:form_query
                               isMainFrame:YES
                            hasUserGesture:YES
                                  webState:web_state()
                         completionHandler:^(BOOL success) {
                           completion_handler_called = YES;
                         }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return completion_handler_called;
    }));
  }

  void LoadHtmlWithRendererInitiatedNavigation(NSString* html,
                                               GURL gurl = GURL()) {
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    context.SetIsRendererInitiated(true);
    [passwordController_.sharedPasswordController webState:web_state()
                                       didFinishNavigation:&context];
    if (gurl.is_empty())
      LoadHtml(html);
    else
      LoadHtml(html, gurl);
  }

  void InjectGeneratedPassword(FormRendererId form_id,
                               FieldRendererId field_id,
                               NSString* password) {
    autofill::PasswordFormGenerationData generation_data = {form_id, field_id,
                                                            FieldRendererId()};
    [passwordController_.sharedPasswordController
        formEligibleForGenerationFound:generation_data];
    __block BOOL block_was_called = NO;
    [passwordController_.sharedPasswordController
        injectGeneratedPasswordForFormId:FormRendererId(1)
                       generatedPassword:password
                       completionHandler:^() {
                         block_was_called = YES;
                       }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return block_was_called;
    }));
    ASSERT_TRUE(
        passwordController_.sharedPasswordController.isPasswordGenerated);
  }

  // SuggestionController for testing.
  PasswordsTestSuggestionController* suggestionController_;

  // FormInputAccessoryMediatorfor testing.
  FormInputAccessoryMediator* accessoryMediator_;

  // PasswordController for testing.
  PasswordController* passwordController_;

  scoped_refptr<password_manager::MockPasswordStore> store_;

  MockPasswordManagerClient* weak_client_;
};

struct FindPasswordFormTestData {
  NSString* html_string;
  const bool expected_form_found;
  // Expected number of fields in found form.
  const size_t expected_number_of_fields;
  // Expected form name.
  const char* expected_form_name;
  const uint32_t maxID;
};

PasswordForm MakeSimpleForm() {
  PasswordForm form;
  form.url = GURL("http://www.google.com/a/LoginAuth");
  form.action = GURL("http://www.google.com/a/Login");
  form.username_element = u"Username";
  form.password_element = u"Passwd";
  form.username_value = u"googleuser";
  form.password_value = u"p4ssword";
  form.signon_realm = "http://www.google.com/";
  form.form_data = MakeSimpleFormData();
  return form;
}

// TODO(crbug.com/403705) This test is flaky.
// Check that HTML forms are converted correctly into FormDatas.
TEST_F(PasswordControllerTest, FLAKY_FindPasswordFormsInView) {
  // clang-format off
  FindPasswordFormTestData test_data[] = {
     // Normal form: a username and a password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user0'>"
      "<input type='password' name='pass0'>"
      "</form>",
      true, 2, "form1", 2
    },
    // User name is captured as an email address (HTML5).
    {
      @"<form name='form1'>"
      "<input type='email' name='email1'>"
      "<input type='password' name='pass1'>"
      "</form>",
      true, 2, "form1", 5
    },
    // No form found.
    {
      @"<div>",
      false, 0, nullptr, 0
    },
    // Disabled username element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user2' disabled='disabled'>"
      "<input type='password' name='pass2'>"
      "</form>",
      true, 2, "form1", 8
    },
    // No password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user3'>"
      "</form>",
      false, 0, nullptr, 0
    },
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string);
    LoadHtml(data.html_string);
    __block std::vector<FormData> forms;
    __block BOOL block_was_called = NO;
    __block uint32_t maxExtractedID;
    [passwordController_.sharedPasswordController.formHelper
        findPasswordFormsWithCompletionHandler:^(
            const std::vector<FormData>& result, uint32_t maxID) {
          block_was_called = YES;
          forms = result;
          maxExtractedID = maxID;
        }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(data.expected_number_of_fields, forms[0].fields.size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(forms[0].name));
    } else {
      ASSERT_TRUE(forms.empty());
    }
    EXPECT_EQ(data.maxID, maxExtractedID);
  }
}

// Test HTML page.  It contains several password forms.  Tests autofill
// them and verify that the right ones are autofilled.
static NSString* kHtmlWithMultiplePasswordForms =
    @""
     // Basic form.
     "<form>"                                      // unique_id 1
     "<input id='un0' type='text' name='u0'>"      // unique_id 2
     "<input id='pw0' type='password' name='p0'>"  // unique_id 3
     "</form>"
     // Form with action in the same origin.
     "<form action='?query=yes#reference'>"        // unique_id 4
     "<input id='un1' type='text' name='u1'>"      // unique_id 5
     "<input id='pw1' type='password' name='p1'>"  // unique_id 6
     "</form>"
     // Form with two exactly same password fields.
     "<form>"                                      // unique_id 7
     "<input id='un2' type='text' name='u2'>"      // unique_id 8
     "<input id='pw2' type='password' name='p2'>"  // unique_id 9
     "<input id='pw2' type='password' name='p2'>"  // unique_id 10
     "</form>"
     // Forms with same names but different ids (1 of 2).
     "<form>"                                      // unique_id 11
     "<input id='un3' type='text' name='u3'>"      // unique_id 12
     "<input id='pw3' type='password' name='p3'>"  // unique_id 13
     "</form>"
     // Forms with same names but different ids (2 of 2).
     "<form>"                                      // unique_id 14
     "<input id='un4' type='text' name='u4'>"      // unique_id 15
     "<input id='pw4' type='password' name='p4'>"  // unique_id 16
     "</form>"
     // Basic form, but with quotes in the names and IDs.
     "<form name=\"f5'\">"                               // unique_id 17
     "<input id=\"un5'\" type='text' name=\"u5'\">"      // unique_id 18
     "<input id=\"pw5'\" type='password' name=\"p5'\">"  // unique_id 19
     "</form>"
     // Fields inside this form don't have name.
     "<form>"                            // unique_id 20
     "<input id='un6' type='text'>"      // unique_id 21
     "<input id='pw6' type='password'>"  // unique_id 22
     "</form>"
     // Fields in this form is attached by form's id.
     "<form id='form7'></form>"                       // unique_id 23
     "<input id='un7' type='text' form='form7'>"      // unique_id 24
     "<input id='pw7' type='password' form='form7'>"  // unique_id 25
     // Fields that are outside the <form> tag.
     "<input id='un8' type='text'>"      // unique_id 26
     "<input id='pw8' type='password'>"  // unique_id 27
                                         // Test forms inside iframes.
     "<iframe id='pf' name='pf'></iframe>"
     "<iframe id='npf' name='npf'></iframe>"
     "<script>"
     "  var doc = frames['pf'].document.open();"
     // Add a form inside iframe. It should also be matched and autofilled.
     "  doc.write('<form id=\\'f10\\'><input id=\\'un10\\' type=\\'text\\' "
     "name=\\'u10\\'>');"  // unique_id 28
     "  doc.write('<input id=\\'pw10\\' type=\\'password\\' name=\\'p10\\'>');"
     "  doc.write('</form>');"  // unique_id 29-28
     // Add a non-password form inside iframe. It should not be matched.
     "  var doc = frames['npf'].document.open();"
     "  doc.write('<form id=\\'f10\\'><input id=\\'un10\\' type=\\'text\\' "
     "name=\\'u10\\'>');"  // unique_id 31
     "  doc.write('<input id=\\'pw10\\' type=\\'text\\' name=\\'p10\\'>');"
     "  doc.write('</form>');"  // unique_id 32-31
     "  doc.close();"
     "</script>";

// A script that resets all text fields, including those in iframes.
static NSString* kClearInputFieldsScript =
    @"function clearInputFields(win) {"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    inputs[i].value = '';"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    clearInputFields(frames[i]);"
     "  }"
     "}"
     "clearInputFields(window);";

// A script that runs after autofilling forms.  It returns ids and values of all
// non-empty fields, including those in iframes.
static NSString* kInputFieldValueVerificationScript =
    @"function findAllInputsInFrame(win, prefix) {"
     "  var result = '';"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    var input = inputs[i];"
     "    if (input.value) {"
     "      result += prefix + input.id + '=' + input.value + ';';"
     "    }"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    result += findAllInputsInFrame("
     "        frames[i], prefix + frames[i].name +'.');"
     "  }"
     "  return result;"
     "};"
     "function findAllInputs(win) {"
     "  return findAllInputsInFrame(win, '');"
     "};"
     "var result = findAllInputs(window); result";

struct FillPasswordFormTestData {
  const std::string origin;
  const char* name;
  uint32_t form_unique_ID;
  const char* username_field;
  uint32_t username_unique_ID;
  const char* username_value;
  const char* password_field;
  uint32_t password_unique_ID;
  const char* password_value;
  const BOOL should_succeed;
  // Expected result generated by |kInputFieldValueVerificationScript|.
  NSString* expected_result;
};

// Tests that filling password forms works correctly.
TEST_F(PasswordControllerTest, FillPasswordForm) {
  LoadHtml(kHtmlWithMultiplePasswordForms);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();
  // clang-format off
  FillPasswordFormTestData test_data[] = {
    // Basic test: one-to-one match on the first password form.
    {
      base_url,
      "gChrome~form~0",
      1,
      "un0",
      2,
      "test_user",
      "pw0",
      3,
      "test_password",
      YES,
      @"un0=test_user;pw0=test_password;"
    },
    // The form matches despite a different action: the only difference
    // is a query and reference.
    {
      base_url,
      "gChrome~form~1",
      4,
      "un1",
      5,
      "test_user",
      "pw1",
      6,
      "test_password",
      YES,
      @"un1=test_user;pw1=test_password;"
    },
    // No match because of a different origin.
    {
      "http://someotherfakedomain.com",
      "gChrome~form~0",
      1,
      "un0",
      2,
      "test_user",
      "pw0",
      3,
      "test_password",
      NO,
      @""
    },
    // No match because some inputs are not in the form.
    {
      base_url,
      "gChrome~form~0",
      1,
      "un0",
      2,
      "test_user",
      "pw1",
      6,
      "test_password",
      NO,
      @""
    },
    // There are inputs with duplicate names in the form, the first of them is
    // filled.
    {
      base_url,
      "gChrome~form~2",
      7,
      "un2",
      8,
      "test_user",
      "pw2",
      9,
      "test_password",
      YES,
      @"un2=test_user;pw2=test_password;"
    },
    // Basic test, but with quotes in the names and IDs.
    {
      base_url,
      "f5'",
      17,
      "un5'",
      18,
      "test_user",
      "pw5'",
      19,
      "test_password",
      YES,
      @"un5'=test_user;pw5'=test_password;"
    },
    // Fields don't have name attributes so id attribute is used for fields
    // identification.
    {
      base_url,
      "gChrome~form~6",
      20,
      "un6",
      21,
      "test_user",
      "pw6",
      22,
      "test_password",
      YES,
      @"un6=test_user;pw6=test_password;"
    },
    // Fields in this form is attached by form's id.
    {
      base_url,
      "form7",
      23,
      "un7",
      24,
      "test_user",
      "pw7",
      25,
      "test_password",
      YES,
      @"un7=test_user;pw7=test_password;"
    },
    // Filling forms inside iframes.
    {
      base_url,
      "f10",
      28,
      "un10",
      29,
      "test_user",
      "pw10",
      30,
      "test_password",
      YES,
      @"pf.un10=test_user;pf.pw10=test_password;"
    },
    // Fields outside the form tag.
    {
      base_url,
      "",
      std::numeric_limits<uint32_t>::max(),
      "un8",
      26,
      "test_user",
      "pw8",
      27,
      "test_password",
      YES,
      @"un8=test_user;pw8=test_password;"
    },
  };
  // clang-format on

  for (const FillPasswordFormTestData& data : test_data) {
    ExecuteJavaScript(kClearInputFieldsScript);

    PasswordFormFillData form_data;
    SetPasswordFormFillData(data.origin, data.name, data.form_unique_ID,
                            data.username_field, data.username_unique_ID,
                            data.username_value, data.password_field,
                            data.password_unique_ID, data.password_value,
                            nullptr, nullptr, false, &form_data);

    __block BOOL block_was_called = NO;
    [passwordController_.sharedPasswordController
         fillPasswordForm:form_data
        completionHandler:^(BOOL success) {
          block_was_called = YES;
          EXPECT_EQ(data.should_succeed, success);
        }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));

    id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
    EXPECT_NSEQ(data.expected_result, result);
  }
}

BOOL PasswordControllerTest::BasicFormFill(NSString* html) {
  LoadHtml(html);
  const std::string base_url = BaseUrl();
  PasswordFormFillData form_data;
  SetPasswordFormFillData(base_url, "gChrome~form~0", 1, "un0", 2, "test_user",
                          "pw0", 3, "test_password", nullptr, nullptr, false,
                          &form_data);
  __block BOOL block_was_called = NO;
  __block BOOL return_value = NO;
  [passwordController_.sharedPasswordController
       fillPasswordForm:form_data
      completionHandler:^(BOOL success) {
        block_was_called = YES;
        return_value = success;
      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  return return_value;
}

// Check that |fillPasswordForm| is not filled if 'readonly' attribute is set
// on either username or password fields.
// TODO(crbug.com/503050): Test is flaky.
TEST_F(PasswordControllerTest, FLAKY_DontFillReadOnly) {
  // Control check that the fill operation will succceed with well-formed form.
  EXPECT_TRUE(BasicFormFill(@"<form>"
                             "<input id='un0' type='text' name='u0'>"
                             "<input id='pw0' type='password' name='p0'>"
                             "</form>"));
  // Form fill should fail with 'readonly' attribute on username.
  EXPECT_FALSE(BasicFormFill(
      @"<form>"
       "<input id='un0' type='text' name='u0' readonly='readonly'>"
       "<input id='pw0' type='password' name='p0'>"
       "</form>"));
  // Form fill should fail with 'readonly' attribute on password.
  EXPECT_FALSE(BasicFormFill(
      @"<form>"
       "<input id='un0' type='text' name='u0'>"
       "<input id='pw0' type='password' name='p0' readonly='readonly'>"
       "</form>"));
}

// TODO(crbug.com/817755): Move them HTML const to separate HTML files.
// An HTML page without a password form.
static NSString* kHtmlWithoutPasswordForm =
    @"<h2>The rain in Spain stays <i>mainly</i> in the plain.</h2>";

// An HTML page containing one password form.  The username input field
// also has custom event handlers.  We need to verify that those event
// handlers are still triggered even though we override them with our own.
static NSString* kHtmlWithPasswordForm =
    @"<form>"
     "<input id='un' type='text' name=\"u'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"p'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "</form>";

static NSString* kHtmlWithNewPasswordForm =
    @"<form>"
     "<input id='un' type='text' name=\"u'\" autocomplete=\"username\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"p'\" autocomplete=\"new-password\">"
     "</form>";

// An HTML page containing two password forms.
static NSString* kHtmlWithTwoPasswordForms =
    @"<form id='f1'>"
     "<input type='text' id='u1'"
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input type='password' id='p1'>"
     "</form>"
     "<form id='f2'>"
     "<input type='text' id='u2'"
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input type='password' id='p2'>"
     "</form>";

// A script that resets indicators used to verify that custom event
// handlers are triggered.  It also finds and the username and
// password fields and caches them for future verification.
static NSString* kUsernameAndPasswordTestPreparationScript =
    @"onKeyUpCalled_ = false;"
     "onChangeCalled_ = false;"
     "username_ = document.getElementById('%@');"
     "username_.__gCrWebAutofilled = 'false';"
     "password_ = document.getElementById('%@');"
     "password_.__gCrWebAutofilled = 'false';";

// A script that we run after autofilling forms.  It returns
// all values for verification as a single concatenated string.
static NSString* kUsernamePasswordVerificationScript =
    @"var value = username_.value;"
     "var from = username_.selectionStart;"
     "var to = username_.selectionEnd;"
     "value.substr(0, from) + '[' + value.substr(from, to) + ']'"
     "   + value.substr(to, value.length) + '=' + password_.value"
     "   + ', onkeyup=' + onKeyUpCalled_"
     "   + ', onchange=' + onChangeCalled_;";

// A script that adds a password form.
static NSString* kAddFormDynamicallyScript =
    @"var dynamicForm = document.createElement('form');"
     "dynamicForm.setAttribute('name', 'dynamic_form');"
     "var inputUsername = document.createElement('input');"
     "inputUsername.setAttribute('type', 'text');"
     "inputUsername.setAttribute('id', 'username');"
     "var inputPassword = document.createElement('input');"
     "inputPassword.setAttribute('type', 'password');"
     "inputPassword.setAttribute('id', 'password');"
     "var submitButton = document.createElement('input');"
     "submitButton.setAttribute('type', 'submit');"
     "submitButton.setAttribute('value', 'Submit');"
     "dynamicForm.appendChild(inputUsername);"
     "dynamicForm.appendChild(inputPassword);"
     "dynamicForm.appendChild(submitButton);"
     "document.body.appendChild(dynamicForm);";

struct SuggestionTestData {
  std::string description;
  NSArray* eval_scripts;
  NSArray* expected_suggestions;
  NSString* expected_result;
};

// Tests that form activity correctly sends suggestions to the suggestion
// controller.
TEST_F(PasswordControllerTest, SuggestionUpdateTests) {
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();
  ExecuteJavaScript(
      [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                 @"un", @"pw"]);

  // Initialize |form_data| with test data and an indicator that autofill
  // should not be performed while the user is entering the username so that
  // we can test with an initially-empty username field. Testing with a
  // username field that contains input is performed by a specific test below.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(base_url, "gChrome~form~0", 1, "un", 2, "user0", "pw",
                          3, "password0", "abc", "def", true, &form_data);

  __block BOOL block_was_called = NO;
  [passwordController_.sharedPasswordController
       fillPasswordForm:form_data
      completionHandler:^(BOOL success) {
        block_was_called = YES;
        // Verify that the fill reports failed.
        EXPECT_FALSE(success);
      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return block_was_called;
  }));

  // Verify that the form has not been autofilled.
  EXPECT_NSEQ(@"[]=, onkeyup=false, onchange=false",
              ExecuteJavaScript(kUsernamePasswordVerificationScript));

  // clang-format off
  SuggestionTestData test_data[] = {
    {
      "Should show all suggestions when focusing empty username field",
      @[(@"var evt = document.createEvent('Events');"
         "username_.focus();"),
        @""],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should show password suggestions when focusing password field",
      @[(@"var evt = document.createEvent('Events');"
         "password_.focus();"),
        @""],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should not filter suggestions when focusing username field with input",
      @[(@"username_.value='ab';"
         "username_.focus();"),
        @""],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"ab[]=, onkeyup=false, onchange=false"
    },
    {
      "Should filter suggestions when typing into a username field",
      @[(@"username_.value='ab';"
         "username_.focus();"
         // Keyup event is dispatched to simulate typing
         "var ev = new KeyboardEvent('keyup', {bubbles:true});"
         "username_.dispatchEvent(ev);"),
        @""],
      @[@"abc ••••••••"],
      @"ab[]=, onkeyup=true, onchange=false"
    },
    {
      "Should not show suggestions when typing into a password field",
      @[(@"username_.value='abc';"
         "password_.value='••';"
         "password_.focus();"
         // Keyup event is dispatched to simulate typing.
         "var ev = new KeyboardEvent('keyup', {bubbles:true});"
         "password_.dispatchEvent(ev);"),
        @""],
      @[],
      @"abc[]=••, onkeyup=true, onchange=false"
    },
  };
  // clang-format on

  for (const SuggestionTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for description=" << data.description
                 << " and eval_scripts=" << data.eval_scripts);
    // Prepare the test.
    ExecuteJavaScript(
        [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                   @"un", @"pw"]);

    for (NSString* script in data.eval_scripts) {
      // Trigger events.
      ExecuteJavaScript(script);

      // Pump the run loop so that the host can respond.
      WaitForBackgroundTasks();
    }
    // Wait until suggestions are received.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return [GetSuggestionValues() count] == [data.expected_suggestions count];
    }));

    EXPECT_NSEQ(data.expected_suggestions, GetSuggestionValues());
    EXPECT_NSEQ(data.expected_result,
                ExecuteJavaScript(kUsernamePasswordVerificationScript));
    // Clear all suggestions.
    [suggestionController_ setSuggestions:nil];
  }
}

// Tests that selecting a suggestion will fill the corresponding form and field.
TEST_F(PasswordControllerTest, SelectingSuggestionShouldFillPasswordForm) {
  LoadHtml(kHtmlWithTwoPasswordForms);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();

  struct TestData {
    const char* form_name;
    const uint32_t form_renderer_id;
    const char* username_element;
    const uint32_t username_renderer_id;
    const char* password_element;
    const uint32_t password_renderer_id;
  } const kTestData[] = {{"f1", 1, "u1", 2, "p1", 3},
                         {"f2", 4, "u2", 5, "p2", 6}};

  // Send fill data to passwordController_.
  for (size_t form_i = 0; form_i < base::size(kTestData); ++form_i) {
    // Initialize |form_data| with test data and an indicator that autofill
    // should not be performed while the user is entering the username so that
    // we can test with an initially-empty username field.
    const auto& test_data = kTestData[form_i];

    PasswordFormFillData form_data;
    SetPasswordFormFillData(
        base_url, test_data.form_name, test_data.form_renderer_id,
        test_data.username_element, test_data.username_renderer_id, "user0",
        test_data.password_element, test_data.password_renderer_id, "password0",
        "abc", "def", true, &form_data);

    __block BOOL block_was_called = NO;
    [passwordController_.sharedPasswordController
         fillPasswordForm:form_data
        completionHandler:^(BOOL success) {
          block_was_called = YES;
          // Verify that the fill reports failed.
          EXPECT_FALSE(success);
        }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
  }

  // Check that the right password form is filled on suggesion selection.
  for (size_t form_i = 0; form_i < base::size(kTestData); ++form_i) {
    const auto& test_data = kTestData[form_i];
    NSString* form_name = SysUTF8ToNSString(test_data.form_name);
    FormRendererId form_renderer_id =
        FormRendererId(test_data.form_renderer_id);
    NSString* username_element = SysUTF8ToNSString(test_data.username_element);
    FieldRendererId username_renderer_id =
        FieldRendererId(test_data.username_renderer_id);
    NSString* password_element = SysUTF8ToNSString(test_data.password_element);

    // Prepare username and passwords for checking.
    ExecuteJavaScript(
        [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                   username_element, password_element]);

    // Verify that the form has not been autofilled.
    EXPECT_NSEQ(@"[]=, onkeyup=false, onchange=false",
                ExecuteJavaScript(kUsernamePasswordVerificationScript));

    std::string mainFrameID = web::GetMainWebFrameId(web_state());
    // Emulate that the user clicks on the username field in the first form.
    // That's required in order that PasswordController can identify which form
    // should be filled.
    __block BOOL block_was_called = NO;
    FormSuggestionProviderQuery* form_query =
        [[FormSuggestionProviderQuery alloc]
            initWithFormName:form_name
                uniqueFormID:form_renderer_id
             fieldIdentifier:username_element
               uniqueFieldID:username_renderer_id
                   fieldType:@"text"
                        type:@"focus"
                  typedValue:@""
                     frameID:SysUTF8ToNSString(mainFrameID)];
    [passwordController_.sharedPasswordController
        retrieveSuggestionsForForm:form_query
                          webState:web_state()
                 completionHandler:^(NSArray* suggestions,
                                     id<FormSuggestionProvider> provider) {
                   NSMutableArray* suggestion_values = [NSMutableArray array];
                   for (FormSuggestion* suggestion in suggestions)
                     [suggestion_values addObject:suggestion.value];
                   EXPECT_NSEQ((@[
                                 @"user0 ••••••••", @"abc ••••••••",
                               ]),
                               suggestion_values);
                   block_was_called = YES;
                 }];
    EXPECT_TRUE(block_was_called);

    // Tell PasswordController that a suggestion was selected. It should fill
    // out the password form with the corresponding credentials.
    FormSuggestion* suggestion =
        [FormSuggestion suggestionWithValue:@"abc ••••••••"
                         displayDescription:nil
                                       icon:nil
                                 identifier:0
                             requiresReauth:NO];

    block_was_called = NO;
    SuggestionHandledCompletion completion = ^{
      block_was_called = YES;
      EXPECT_NSEQ(@"abc[]=def, onkeyup=true, onchange=true",
                  ExecuteJavaScript(kUsernamePasswordVerificationScript));
    };
    [passwordController_.sharedPasswordController
        didSelectSuggestion:suggestion
                       form:SysUTF8ToNSString(FormName(0))
               uniqueFormID:form_renderer_id
            fieldIdentifier:@"u"
              uniqueFieldID:username_renderer_id
                    frameID:SysUTF8ToNSString(mainFrameID)
          completionHandler:completion];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
  }
}

// The test case below needs to use MockWebState, thus it needs a different
// SetUp.
class PasswordControllerTestSimple : public PlatformTest {
 public:
  PasswordControllerTestSimple()
      : store_(new testing::NiceMock<password_manager::MockPasswordStore>()) {}

  ~PasswordControllerTestSimple() override { store_->ShutdownOnUIThread(); }

  void SetUp() override {
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

    std::unique_ptr<TestChromeBrowserState> browser_state(builder.Build());
    ON_CALL(web_state_, GetBrowserState)
        .WillByDefault(testing::Return(browser_state.get()));
    UniqueIDDataTabHelper::CreateForWebState(&web_state_);

    passwordController_ =
        CreatePasswordController(&web_state_, store_.get(), &weak_client_);

    ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
        .WillByDefault(Return(true));

    ON_CALL(*store_, GetLogins)
        .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  }

  PasswordController* passwordController_;
  scoped_refptr<password_manager::MockPasswordStore> store_;
  MockPasswordManagerClient* weak_client_;
  MockWebState web_state_;
  base::test::TaskEnvironment task_environment;
  TestChromeBrowserState::Builder builder;
  std::unique_ptr<TestChromeBrowserState> browser_state;
};

TEST_F(PasswordControllerTestSimple, SaveOnNonHTMLLandingPage) {
  // Have a form observed and submitted.
  FormData formData = MakeSimpleFormData();
  SharedPasswordController* sharedPasswordController =
      passwordController_.sharedPasswordController;
  [sharedPasswordController formHelper:sharedPasswordController.formHelper
                         didSubmitForm:formData
                           inMainFrame:YES];

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Save password prompt shpuld be shown after navigation to a non-HTML page.
  web_state_.SetContentIsHTML(false);
  web_state_.SetCurrentURL(GURL("https://google.com/success"));
  [sharedPasswordController webState:&web_state_ didLoadPageWithSuccess:YES];

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));

  EXPECT_EQ("http://www.google.com/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"googleuser",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"p4ssword",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Checks that when the user set a focus on a field of a password form which was
// not sent to the store then the request the the store is sent.
TEST_F(PasswordControllerTest, SendingToStoreDynamicallyAddedFormsOnFocus) {
  LoadHtml(kHtmlWithoutPasswordForm);
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(1);");
  ExecuteJavaScript(kAddFormDynamicallyScript);

  // The standard pattern is to use a __block variable WaitUntilCondition but
  // __block variable can't be captured in C++ lambda, so as workaround it's
  // used normal variable |get_logins_called| and pointer on it is used in a
  // block.
  bool get_logins_called = false;
  bool* p_get_logins_called = &get_logins_called;

  password_manager::PasswordStore::FormDigest expected_form_digest(
      password_manager::PasswordForm::Scheme::kHtml, "https://chromium.test/",
      GURL("https://chromium.test/"));
  // TODO(crbug.com/949519): replace WillRepeatedly with WillOnce when the old
  // parser is gone.
  EXPECT_CALL(*store_, GetLogins(expected_form_digest, _))
      .WillRepeatedly(testing::Invoke(
          [&get_logins_called](
              const password_manager::PasswordStore::FormDigest&,
              password_manager::PasswordStoreConsumer*) {
            get_logins_called = true;
          }));

  // Sets a focus on a username field.
  NSString* kSetUsernameInFocusScript =
      @"document.getElementById('username').focus();";
  ExecuteJavaScript(kSetUsernameInFocusScript);

  // Wait until GetLogins is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return *p_get_logins_called;
  }));
}

// Tests that a touchend event from a button which contains in a password form
// works as a submission indicator for this password form.
TEST_F(PasswordControllerTest, TouchendAsSubmissionIndicator) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  const char* kHtml[] = {
      "<html><body>"
      "<form name='login_form' id='login_form'>"
      "  <input type='text' name='username'>"
      "  <input type='password' name='password'>"
      "  <button id='submit_button' value='Submit'>"
      "</form>"
      "</body></html>",
      "<html><body>"
      "<form name='login_form' id='login_form'>"
      "  <input type='text' name='username'>"
      "  <input type='password' name='password'>"
      "  <button id='back' value='Back'>"
      "  <button id='submit_button' type='submit' value='Submit'>"
      "</form>"
      "</body></html>"};

  for (size_t i = 0; i < base::size(kHtml); ++i) {
    LoadHtml(SysUTF8ToNSString(kHtml[i]));
    WaitForFormManagersCreation();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    ExecuteJavaScript(
        @"document.getElementsByName('username')[0].value = 'user1';"
         "document.getElementsByName('password')[0].value = 'password1';"
         "var e = new UIEvent('touchend');"
         "document.getElementById('submit_button').dispatchEvent(e);");
    LoadHtmlWithRendererInitiatedNavigation(
        SysUTF8ToNSString("<html><body>Success</body></html>"));

    auto& form_manager_check = form_manager_to_save;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return form_manager_check != nullptr;
    }));

    EXPECT_EQ("https://chromium.test/",
              form_manager_to_save->GetPendingCredentials().signon_realm);
    EXPECT_EQ(u"user1",
              form_manager_to_save->GetPendingCredentials().username_value);
    EXPECT_EQ(u"password1",
              form_manager_to_save->GetPendingCredentials().password_value);

    auto* form_manager =
        static_cast<PasswordFormManager*>(form_manager_to_save.get());
    EXPECT_TRUE(form_manager->is_submitted());
    EXPECT_FALSE(form_manager->IsPasswordUpdate());
  }
}

// Tests that a touchend event from a button which contains in a password form
// works as a submission indicator for this password form.
TEST_F(PasswordControllerTest, SavingFromSameOriginIframe) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  LoadHtml(@"<iframe id='frame1' name='frame1'></iframe>");
  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.body.innerHTML = "
       "'<form id=\"form1\">"
       "<input type=\"text\" name=\"text\" value=\"user1\" id=\"id2\">"
       "<input type=\"password\" name=\"password\" value=\"pw1\" id=\"id2\">"
       "<input type=\"submit\" id=\"submit_input\"/>"
       "</form>'");
  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.getElementById('"
      @"submit_input').click();");

  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"pw1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that when a dynamic form added and the user clicks on the username
// field in this form, then the request to the Password Store is sent and
// PassworController is waiting to the response in order to show or not to show
// password suggestions.
TEST_F(PasswordControllerTest, CheckAsyncSuggestions) {
  for (bool store_has_credentials : {false, true}) {
    if (store_has_credentials) {
      PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "user", "pw"));
      // TODO(crbug.com/949519): replace WillRepeatedly with WillOnce when the
      // old parser is gone.
      EXPECT_CALL(*store_, GetLogins)
          .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
    } else {
      EXPECT_CALL(*store_, GetLogins)
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    }
    LoadHtml(kHtmlWithoutPasswordForm);
    ExecuteJavaScript(kAddFormDynamicallyScript);

    SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                       FieldRendererId(), std::string());
    WaitForFormManagersCreation();

    __block BOOL completion_handler_success = NO;
    __block BOOL completion_handler_called = NO;

    FormRendererId form_id =
        store_has_credentials ? FormRendererId(4) : FormRendererId(1);
    FieldRendererId field_id =
        store_has_credentials ? FieldRendererId(5) : FieldRendererId(2);
    std::string mainFrameID = web::GetMainWebFrameId(web_state());

    FormSuggestionProviderQuery* form_query =
        [[FormSuggestionProviderQuery alloc]
            initWithFormName:@"dynamic_form"
                uniqueFormID:form_id
             fieldIdentifier:@"username"
               uniqueFieldID:field_id
                   fieldType:@"text"
                        type:@"focus"
                  typedValue:@""
                     frameID:SysUTF8ToNSString(mainFrameID)];
    [passwordController_.sharedPasswordController
        checkIfSuggestionsAvailableForForm:form_query
                               isMainFrame:YES
                            hasUserGesture:YES
                                  webState:web_state()
                         completionHandler:^(BOOL success) {
                           completion_handler_success = success;
                           completion_handler_called = YES;
                         }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return completion_handler_called;
    }));

    EXPECT_EQ(store_has_credentials, completion_handler_success);
    testing::Mock::VerifyAndClearExpectations(&store_);
  }
}

// Tests that when a dynamic form added and the user clicks on non username
// field in this form, then the request to the Password Store is sent but no
// suggestions are shown.
TEST_F(PasswordControllerTest, CheckNoAsyncSuggestionsOnNonUsernameField) {
  PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "user", "pw"));
  EXPECT_CALL(*store_, GetLogins).WillOnce(WithArg<1>(InvokeConsumer(form)));

  LoadHtml(kHtmlWithoutPasswordForm);
  ExecuteJavaScript(kAddFormDynamicallyScript);

  SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                     FieldRendererId(), std::string());
  WaitForFormManagersCreation();

  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"dynamic_form"
          uniqueFormID:FormRendererId(1)
       fieldIdentifier:@"address"
         uniqueFieldID:FieldRendererId(4)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:form_query
                             isMainFrame:YES
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         completion_handler_success = success;
                         completion_handler_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return completion_handler_called;
  }));

  EXPECT_FALSE(completion_handler_success);
}

// Tests that when there are no password forms on a page and the user clicks on
// a text field the completion callback is called with no suggestions result.
TEST_F(PasswordControllerTest, CheckNoAsyncSuggestionsOnNoPasswordForms) {
  LoadHtml(kHtmlWithoutPasswordForm);

  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;

  EXPECT_CALL(*store_, GetLogins).Times(0);
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:FormRendererId(1)
       fieldIdentifier:@"address"
         uniqueFieldID:FieldRendererId(2)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:form_query
                             isMainFrame:YES
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         completion_handler_success = success;
                         completion_handler_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return completion_handler_called;
  }));

  EXPECT_FALSE(completion_handler_success);
}

// Tests password generation suggestion is shown properly.
TEST_F(PasswordControllerTest, CheckPasswordGenerationSuggestion) {
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_CALL(*weak_client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(Return(true));

  LoadHtml(kHtmlWithNewPasswordForm);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();
  ExecuteJavaScript(
      [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                 @"un", @"pw"]);

  // Initialize |form_data| with test data and an indicator that autofill
  // should not be performed while the user is entering the username so that
  // we can test with an initially-empty username field. Testing with a
  // username field that contains input is performed by a specific test below.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(base_url, "gChrome~form~0", 1, "un", 2, "user0", "pw",
                          3, "password0", "abc", "def", true, &form_data);

  __block BOOL block_was_called = NO;
  [passwordController_.sharedPasswordController
       fillPasswordForm:form_data
      completionHandler:^(BOOL success) {
        block_was_called = YES;
        // Verify that the fill reports failed.
        EXPECT_FALSE(success);
      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return block_was_called;
  }));

  // Verify that the form has not been autofilled.
  EXPECT_NSEQ(@"[]=, onkeyup=false, onchange=false",
              ExecuteJavaScript(kUsernamePasswordVerificationScript));

  // clang-format off
  SuggestionTestData test_data[] = {
    {
      "Should not show suggest password when focusing username field",
      @[(@"var evt = document.createEvent('Events');"
         "username_.focus();"),
        @""],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should show suggest password when focusing password field",
      @[(@"var evt = document.createEvent('Events');"
         "password_.focus();"),
        @""],
      @[@"user0 ••••••••", @"abc ••••••••", @"Suggest Password\u2026"],
      @"[]=, onkeyup=false, onchange=false"
    },
  };
  // clang-format on

  for (const SuggestionTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for description=" << data.description
                 << " and eval_scripts=" << data.eval_scripts);
    // Prepare the test.
    ExecuteJavaScript(
        [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                   @"un", @"pw"]);

    for (NSString* script in data.eval_scripts) {
      // Trigger events.
      ExecuteJavaScript(script);

      // Pump the run loop so that the host can respond.
      WaitForBackgroundTasks();
    }
    // Wait until suggestions are received.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return [GetSuggestionValues() count] > 0;
    }));

    EXPECT_NSEQ(data.expected_suggestions, GetSuggestionValues());
    EXPECT_NSEQ(data.expected_result,
                ExecuteJavaScript(kUsernamePasswordVerificationScript));
    // Clear all suggestions.
    [suggestionController_ setSuggestions:nil];
  }
}


// Check that if the PasswordController is told (by the PasswordManagerClient)
// that this is Incognito, it won't enable password generation.
TEST_F(PasswordControllerTest, IncognitoPasswordGenerationDisabled) {
    TearDown();
    ChromeWebTest::SetUp();

    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

    auto client =
    std::make_unique<NiceMock<MockPasswordManagerClient>>(store_.get());
    weak_client_ = client.get();

    EXPECT_CALL(*weak_client_->GetPasswordFeatureManager(), IsGenerationEnabled)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*weak_client_, IsIncognito).WillRepeatedly(Return(true));

    UniqueIDDataTabHelper::CreateForWebState(web_state());
    passwordController_ =
    [[PasswordController alloc] initWithWebState:web_state()
                                          client:std::move(client)];

    EXPECT_FALSE(passwordController_.passwordManagerDriver
                     ->GetPasswordGenerationHelper());
}

// Tests that the user is prompted to save or update password on a succesful
// form submission.
TEST_F(PasswordControllerTest, ShowingSavingPromptOnSuccessfulSubmission) {
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "document.getElementById('submit_button').click();");
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that the user is not prompted to save or update password on
// leaving the page before submitting the form.
TEST_F(PasswordControllerTest, NotShowingSavingPromptWithoutSubmission) {
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);
  ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';");
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>New page</body></html>"));
}

// Tests that the user is not prompted to save or update password on a
// succesful form submission while saving is disabled.
TEST_F(PasswordControllerTest, NotShowingSavingPromptWhileSavingIsDisabled) {
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
      .WillByDefault(Return(false));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);
  ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "document.getElementById('submit_button').click();");
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
}

// Tests that the user is prompted to update password on a succesful
// form submission when there's already a credential with the same
// username in the store.
TEST_F(PasswordControllerTest, ShowingUpdatePromptOnSuccessfulSubmission) {
  PasswordForm form(MakeSimpleForm());
  ON_CALL(*store_, GetLogins).WillByDefault(WithArg<1>(InvokeConsumer(form)));
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='Username'>"
                       "  <input type='password' name='Passwd'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};

  LoadHtml(SysUTF8ToNSString(kHtml), GURL("http://www.google.com/a/LoginAuth"));
  WaitForFormManagersCreation();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  ExecuteJavaScript(
      @"document.getElementsByName('Username')[0].value = 'googleuser';"
       "document.getElementsByName('Passwd')[0].value = 'new_password';"
       "document.getElementById('submit_button').click();");
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"),
      GURL("http://www.google.com/a/Login"));

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("http://www.google.com/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"googleuser",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"new_password",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_TRUE(form_manager->IsPasswordUpdate());
}

TEST_F(PasswordControllerTest, SavingOnNavigateMainFrame) {
  constexpr char kHtml[] = "<html><body>"
                           "<form name='login_form' id='login_form'>"
                           "  <input type='text' name='username'>"
                           "  <input type='password' name='pw'>"
                           "</form>"
                           "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(1);");
  FormRendererId form_id = FormRendererId(1);
  FieldRendererId username_id = FieldRendererId(2);
  FieldRendererId password_id = FieldRendererId(3);
  for (bool has_commited : {false, true}) {
    for (bool is_same_document : {false, true}) {
      for (bool is_renderer_initiated : {false, true}) {
        SCOPED_TRACE(testing::Message("has_commited = ")
                     << has_commited << " is_same_document=" << is_same_document
                     << " is_renderer_initiated=" << is_renderer_initiated);
        LoadHtml(SysUTF8ToNSString(kHtml));

        auto& form_managers =
            passwordController_.passwordManager->form_managers();
        ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
          return !form_managers.empty();
        }));

        std::string main_frame_id = web::GetMainWebFrameId(web_state());

        SimulateUserTyping("login_form", form_id, "username", username_id,
                           "user1", main_frame_id);
        SimulateUserTyping("login_form", form_id, "pw", password_id,
                           "password1", main_frame_id);

        bool prompt_should_be_shown =
            has_commited && !is_same_document && is_renderer_initiated;

        std::unique_ptr<PasswordFormManagerForUI> form_manager;
        if (prompt_should_be_shown) {
          EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
              .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));
        } else {
          EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
              .Times(0);
        }
        form_id.value() += 3;
        username_id.value() += 3;
        password_id.value() += 3;
        web::FakeNavigationContext context;
        context.SetHasCommitted(has_commited);
        context.SetIsSameDocument(is_same_document);
        context.SetIsRendererInitiated(is_renderer_initiated);
        [passwordController_.sharedPasswordController webState:web_state()
                                           didFinishNavigation:&context];

        // Simulate a successful submission by loading the landing page without
        // a form.
        LoadHtml(
            SysUTF8ToNSString("<html><body>Login success page</body></html>"));

        if (prompt_should_be_shown) {
          auto& form_manager_check = form_manager;
          ASSERT_TRUE(
              WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
                return form_manager_check != nullptr;
              }));
          EXPECT_EQ(u"user1",
                    form_manager->GetPendingCredentials().username_value);
          EXPECT_EQ(u"password1",
                    form_manager->GetPendingCredentials().password_value);
        }
        testing::Mock::VerifyAndClearExpectations(weak_client_);
      }
    }
  }
}

TEST_F(PasswordControllerTest, NoSavingOnNavigateMainFrameFailedSubmission) {
  constexpr char kHtml[] = "<html><body>"
                           "<form name='login_form' id='login_form'>"
                           "  <input type='text' name='username'>"
                           "  <input type='password' name='pw'>"
                           "</form>"
                           "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("login_form", FormRendererId(1), "username",
                     FieldRendererId(2), "user1", main_frame_id);
  SimulateUserTyping("login_form", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", main_frame_id);

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  context.SetIsRendererInitiated(true);
  [passwordController_.sharedPasswordController webState:web_state()
                                     didFinishNavigation:&context];

  // Simulate a failed submission by loading the same form again.
  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();
}

// Tests that a form that is dynamically added to the page is found and
// that a form manager is created for it.
TEST_F(PasswordControllerTest, FindDynamicallyAddedForm2) {
  LoadHtml(kHtmlWithoutPasswordForm);
  ExecuteJavaScript(kAddFormDynamicallyScript);

  SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                     FieldRendererId(), std::string());
  WaitForFormManagersCreation();

  auto& form_managers = passwordController_.passwordManager->form_managers();
  ASSERT_EQ(1u, form_managers.size());
  auto* password_form = form_managers[0]->observed_form();
  EXPECT_EQ(u"dynamic_form", password_form->name);
}

// Tests that submission is detected on removal of the form that had user input.
TEST_F(PasswordControllerTest, DetectSubmissionOnRemovedForm) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  std::string mainFrameID = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("login_form", FormRendererId(1), "username",
                     FieldRendererId(2), "user1", mainFrameID);
  SimulateUserTyping("login_form", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", mainFrameID);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  SimulateFormActivityObserverSignal("password_form_removed", FormRendererId(1),
                                     FieldRendererId(), std::string());

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that submission is not detected on removal of the form that never
// had user input.
TEST_F(PasswordControllerTest,
       DetectNoSubmissionOnRemovedFormWithoutUserInput) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  SimulateFormActivityObserverSignal("password_form_removed", FormRendererId(1),
                                     FieldRendererId(), std::string());
}

// Tests that submission is detected on removal of the form that had user input.
TEST_F(PasswordControllerTest, DetectSubmissionOnIFrameDetach) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_TRUE(
      LoadHtml("<script>"
               "  function FillFrame() {"
               "       var doc = frames['frame1'].document.open();"
               "       doc.write('<form id=\"form1\">');"
               "       doc.write('<input id=\"un\" type=\"text\">');"
               "       doc.write('<input id=\"pw\" type=\"password\">');"
               "       doc.write('</form>');"
               "       doc.close();"
               // This event listerer is set by Chrome, but it gets disabled
               // by document.write(). This is quite uncommon way to add
               // content to an iframe, but it is the only way for this test.
               // Reattaching it manually for test purposes.
               "       frames[0].addEventListener('unload', function(event) {"
               "  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',"
               "      frames[0].__gCrWeb.message.getFrameId());"
               "});"
               "}"
               "</script>"
               "<body onload='FillFrame()'>"
               "<iframe id='frame1' name='frame1'></iframe>"
               "</body>"));

  WaitForFormManagersCreation();

  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  std::set<WebFrame*> all_frames =
      web_state()->GetWebFramesManager()->GetAllWebFrames();
  std::string iFrameID;
  for (auto* frame : all_frames) {
    if (!frame->IsMainFrame()) {
      iFrameID = frame->GetFrameId();
      break;
    }
  }

  SimulateUserTyping("form1", FormRendererId(1), "un", FieldRendererId(2),
                     "user1", iFrameID);
  SimulateUserTyping("form1", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", iFrameID);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  ExecuteJavaScript(@"var frame1 = document.getElementById('frame1');"
                     "frame1.parentNode.removeChild(frame1);");
  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));

  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that no submission is detected on removal of the form that had no user
// input.
TEST_F(PasswordControllerTest,
       DetectNoSubmissionOnIFrameDetachWithoutUserInput) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_TRUE(
      LoadHtml("<script>"
               "  function FillFrame() {"
               "       var doc = frames['frame1'].document.open();"
               "       doc.write('<form id=\"form1\">');"
               "       doc.write('<input id=\"un\" type=\"text\">');"
               "       doc.write('<input id=\"pw\" type=\"password\">');"
               "       doc.write('</form>');"
               "       doc.close();"
               // This event listerer is set by Chrome, but it gets disabled
               // by document.write(). This is quite uncommon way to add
               // content to an iframe, but it is the only way for this test.
               // Reattaching it manually for test purposes.
               "       frames[0].addEventListener('unload', function(event) {"
               "  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',"
               "      frames[0].__gCrWeb.message.getFrameId());"
               "});"
               "}"
               "</script>"
               "<body onload='FillFrame()'>"
               "<iframe id='frame1' name='frame1'></iframe>"
               "</body>"));

  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  ExecuteJavaScript(@"var frame1 = document.getElementById('frame1');"
                     "frame1.parentNode.removeChild(frame1);");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    auto frames = web_state()->GetWebFramesManager()->GetAllWebFrames();
    return frames.size() == 1;
  }));
}

TEST_F(PasswordControllerTest, PasswordMetricsNoSavedCredentials) {
  base::HistogramTester histogram_tester;
  {
    ON_CALL(*store_, GetLogins)
        .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));
    LoadHtml(@"<html><body>"
              "<form name='login_form' id='login_form'>"
              "  <input type='text' name='username'>"
              "  <input type='password' name='password'>"
              "  <button id='submit_button' value='Submit'>"
              "</form>"
              "</body></html>");
    WaitForFormManagersCreation();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    std::string main_frame_id = web::GetMainWebFrameId(web_state());
    ExecuteJavaScript(
        @"document.getElementsByName('username')[0].value = 'user';"
         "document.getElementsByName('password')[0].value = 'pw';"
         "document.getElementById('submit_button').click();");
    LoadHtmlWithRendererInitiatedNavigation(
        @"<html><body>Success</body></html>");

    auto& form_manager_check = form_manager_to_save;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return form_manager_check != nullptr;
    }));
  }

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                      FillingAssistance::kNoSavedCredentials,
                                      1);
}

TEST_F(PasswordControllerTest, PasswordMetricsAutomatic) {
  base::HistogramTester histogram_tester;

  passwordController_.sharedPasswordController.formHelper.jsPasswordManager =
      [[FakeJsPasswordManager alloc] init];

  PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "user", "pw"));
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  LoadHtml(@"<html><body>"
            "<form name='login_form' id='login_form'>"
            "  <input type='text' name='username'>"
            "  <input type='password' name='password'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  PasswordFormFillData form_data;
  SetPasswordFormFillData(BaseUrl(), "login_form", 1, "username", 2, "user",
                          "password", 3, "pw", nullptr, nullptr, false,
                          &form_data);
  __block BOOL block_was_called = NO;
  __block BOOL return_value = NO;
  [passwordController_.sharedPasswordController
       fillPasswordForm:form_data
      completionHandler:^(BOOL success) {
        block_was_called = YES;
        return_value = success;
      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return block_was_called;
  }));

  ExecuteJavaScript(
      @"var e = new UIEvent('touchend');"
       "document.getElementById('submit_button').dispatchEvent(e);");
  LoadHtmlWithRendererInitiatedNavigation(@"<html><body>Success</body></html>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return passwordController_.sharedPasswordController.formHelper
        .jsPasswordManager.noFormsSeen;
  }));

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                      FillingAssistance::kAutomatic, 1);
}

// Tests that focusing the password field containing the generated password
// is not breaking the password generation flow.
// Verifies the fix for crbug.com/1077271.
TEST_F(PasswordControllerTest, PasswordGenerationFieldFocus) {
  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Focus the password field after password generation.
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  __block bool block_was_called = NO;
  FormSuggestionProviderQuery* focus_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"focus"
                typedValue:@""
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:focus_query
                             isMainFrame:YES
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is still generated.
  ASSERT_TRUE(passwordController_.sharedPasswordController.isPasswordGenerated);
}

// Tests that adding input into the password field containing the generated
// password is not breaking the password generation flow.
TEST_F(PasswordControllerTest, PasswordGenerationFieldInput) {
  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Extend the password after password generation.
  __block bool block_was_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* extend_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"input"
                typedValue:@"generated_password_long"
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:extend_query
                             isMainFrame:YES
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is still considered generated.
  ASSERT_TRUE(passwordController_.sharedPasswordController.isPasswordGenerated);
}

// Tests that clearing the value of the password field containing
// the generated password stops the generation flow.
TEST_F(PasswordControllerTest, PasswordGenerationFieldClear) {
  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Clear the password.
  __block bool block_was_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* clear_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"input"
                typedValue:@""
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:clear_query
                             isMainFrame:YES
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is not considered generated anymore.
  ASSERT_FALSE(
      passwordController_.sharedPasswordController.isPasswordGenerated);
}

TEST_F(PasswordControllerTest, SavingPasswordsOutsideTheFormTag) {
  NSString* kHtml = @"<html><body>"
                     "<input type='text' name='username'>"
                     "<input type='password' name='pw'>"
                     "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms()));

  LoadHtml(kHtml);
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());
  SimulateUserTyping("", FormRendererId(), "username", FieldRendererId(1),
                     "user1", main_frame_id);
  SimulateUserTyping("", FormRendererId(), "pw", FieldRendererId(2),
                     "password1", main_frame_id);

  __block std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));

  // Simulate a renderer initiated navigation.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsRendererInitiated(true);
  [passwordController_.sharedPasswordController webState:web_state()
                                     didFinishNavigation:&context];

  // Simulate a successful submission by loading the landing page without
  // a form.
  LoadHtml(@"<html><body>Login success page</body></html>");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager != nullptr;
  }));
  EXPECT_EQ(u"user1", form_manager->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1", form_manager->GetPendingCredentials().password_value);
}

// Tests that submission is detected on change password form clearing.
TEST_F(PasswordControllerTest, DetectSubmissionOnFormReset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kDetectFormSubmissionOnFormClear);

  PasswordForm form(
      CreatePasswordForm("https://chromium.test/", "user", "oldpw"));
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  LoadHtml(@"<html><body>"
            "<form name='change_form' id='change_form'>"
            "  <input type='password' id='opw'>"
            "  <input type='password' id='npw' autocomplete='new-password'>"
            "  <input type='password' id='cpw' autocomplete='new-password'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("change_form", FormRendererId(1), "opw",
                     FieldRendererId(2), "oldpw", main_frame_id);
  SimulateUserTyping("change_form", FormRendererId(1), "npw",
                     FieldRendererId(3), "newpw", main_frame_id);
  SimulateUserTyping("change_form", FormRendererId(1), "cpw",
                     FieldRendererId(4), "newpw", main_frame_id);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  std::string form_data = base::SysNSStringToUTF8(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.getPasswordFormDataAsString(%d);",
                       1]));

  // Imitiate the signal from the page resetting the form.
  SimulateFormActivityObserverSignal("password_form_cleared", FormRendererId(1),
                                     FieldRendererId(), form_data);

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"newpw",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_TRUE(form_manager->IsPasswordUpdate());
}

// Tests that submission is detected on change password form clearing,
// when the formless fields are cleared individually.
TEST_F(PasswordControllerTest, DetectSubmissionOnFormlessFieldsClearing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kDetectFormSubmissionOnFormClear);

  PasswordForm form(
      CreatePasswordForm("https://chromium.test/", "user", "oldpw"));
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  LoadHtml(@"<html><body>"
            "  <input type='password' id='opw'>"
            "  <input type='password' id='npw' autocomplete='new-password'>"
            "  <input type='password' id='cpw' autocomplete='new-password'>"
            "  <button id='submit_button' value='Submit'>"
            "</body></html>");
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("change_form", FormRendererId(), "opw", FieldRendererId(1),
                     "oldpw", main_frame_id);
  SimulateUserTyping("change_form", FormRendererId(), "npw", FieldRendererId(2),
                     "newpw", main_frame_id);
  SimulateUserTyping("change_form", FormRendererId(), "cpw", FieldRendererId(3),
                     "newpw", main_frame_id);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  std::string form_data = base::SysNSStringToUTF8(
      ExecuteJavaScript(@"__gCrWeb.passwords.getPasswordFormDataAsString(0);"));

  // Imitiate the signal from the page resetting the form.
  SimulateFormActivityObserverSignal("password_form_cleared", FormRendererId(),
                                     FieldRendererId(), form_data);

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"newpw",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_TRUE(form_manager->IsPasswordUpdate());
}
