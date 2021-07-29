// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF16;
using base::UTF16ToUTF8;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::JsonStringToFormData;

namespace password_manager {
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state,
                                  GURL* __nullable page_url) {
  auto trustLevel = web::URLVerificationTrustLevel::kNone;
  GURL dummy;
  if (!page_url) {
    page_url = &dummy;
  }
  *page_url = web_state->GetCurrentURL(&trustLevel);
  return trustLevel == web::URLVerificationTrustLevel::kAbsolute;
}
}  // namespace password_manager

namespace {
// Script command prefix for form changes. Possible command to be sent from
// injected JS is 'passwordForm.submitButtonClick'.
constexpr char kCommandPrefix[] = "passwordForm";
}  // namespace

@interface PasswordFormHelper ()

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::Value&)JSONCommand;

// Parses the |jsonString| which contatins the password forms found on a web
// page to populate the |forms| vector.
- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)jsonString
                 pageURL:(const GURL&)pageURL;

@end

@implementation PasswordFormHelper {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Subscription for JS message.
  base::CallbackListSubscription _subscription;
}

#pragma mark - Properties

@synthesize fieldDataManager = _fieldDataManager;

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);

    UniqueIDDataTabHelper* uniqueIDDataTabHelper =
        UniqueIDDataTabHelper::FromWebState(_webState);
    _fieldDataManager = uniqueIDDataTabHelper->GetFieldDataManager();

    __weak PasswordFormHelper* weakSelf = self;
    auto callback =
        base::BindRepeating(^(const base::Value& JSON, const GURL& originURL,
                              bool interacting, web::WebFrame* senderFrame) {
          // Passwords is only supported on main frame.
          if (senderFrame->IsMainFrame()) {
            // |originURL| and |interacting| aren't used.
            [weakSelf handleScriptCommand:JSON];
          }
        });
    _subscription =
        _webState->AddScriptCommandCallback(callback, kCommandPrefix);
  }
  return self;
}

#pragma mark - Dealloc

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  _formActivityObserverBridge.reset();
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                   formInMainFrame:(BOOL)formInMainFrame
                           inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  GURL pageURL = webState->GetLastCommittedURL();
  if (pageURL.GetOrigin() != frame->GetSecurityOrigin()) {
    // Passwords is only supported on main frame and iframes with the same
    // origin.
    return;
  }
  if (!self.delegate || formData.empty()) {
    return;
  }
  std::vector<FormData> forms;
  NSString* nsFormData = [NSString stringWithUTF8String:formData.c_str()];
  autofill::ExtractFormsData(nsFormData, false, std::u16string(), pageURL,
                             pageURL.GetOrigin(), &forms);
  if (forms.size() != 1) {
    return;
  }

  // Extract FieldDataManager data for observed fields.
  [self extractKnownFieldData:forms[0]];

  [self.delegate formHelper:self
              didSubmitForm:forms[0]
                inMainFrame:formInMainFrame];
}

#pragma mark - Private methods

- (BOOL)handleScriptCommand:(const base::Value&)JSONCommand {
  const std::string* command = JSONCommand.FindStringKey("command");
  if (!command || *command != "passwordForm.submitButtonClick") {
    return NO;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    return NO;
  }

  FormData form;
  if (!autofill::ExtractFormData(JSONCommand, false, std::u16string(), pageURL,
                                 pageURL.GetOrigin(), &form)) {
    return NO;
  }

  // Extract FieldDataManager data for observed fields.
  [self extractKnownFieldData:form];

  if (_webState && self.delegate) {
    [self.delegate formHelper:self didSubmitForm:form inMainFrame:YES];
    return YES;
  }

  return NO;
}

- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)JSONString
                 pageURL:(const GURL&)pageURL {
  std::vector<FormData> formsData;
  if (!autofill::ExtractFormsData(JSONString, false, std::u16string(), pageURL,
                                  pageURL.GetOrigin(), &formsData)) {
    return;
  }
  // Extract FieldDataManager data for observed form fields.
  for (FormData& form : formsData) {
    [self extractKnownFieldData:form];
  }
  *forms = std::move(formsData);
}

// Extracts known field data.
- (void)extractKnownFieldData:(FormData&)form {
  for (auto& field : form.fields) {
    if (self.fieldDataManager->HasFieldData(field.unique_renderer_id)) {
      field.user_input =
          self.fieldDataManager->GetUserInput(field.unique_renderer_id);
      field.properties_mask = self.fieldDataManager->GetFieldPropertiesMask(
          field.unique_renderer_id);
    }
  }
}

#pragma mark - Public methods

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<FormData>&, uint32_t))completionHandler {
  if (!_webState) {
    return;
  }

  web::WebFrame* mainFrame = web::GetMainFrame(_webState);
  if (!mainFrame) {
    return;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    return;
  }

  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FindPasswordFormsInFrame(
          mainFrame, base::BindOnce(^(NSString* JSONString) {
            std::vector<FormData> forms;
            [weakSelf getPasswordForms:&forms
                              fromJSON:JSONString
                               pageURL:pageURL];
            // Find the maximum extracted value.
            uint32_t maxID = 0;
            for (const auto& form : forms) {
              if (form.unique_renderer_id) {
                maxID = std::max(maxID, form.unique_renderer_id.value());
              }
              for (const auto& field : form.fields) {
                if (field.unique_renderer_id) {
                  maxID = std::max(maxID, field.unique_renderer_id.value());
                }
              }
            }
            completionHandler(forms, maxID);
          }));
}

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(nullable void (^)(BOOL))completionHandler {
  web::WebFrame* mainFrame = web::GetMainFrame(_webState);
  if (!mainFrame) {
    return;
  }

  // Necessary copy so the values can be used inside a block.
  FieldRendererId usernameID = formData.username_field.unique_renderer_id;
  FieldRendererId passwordID = formData.password_field.unique_renderer_id;
  std::u16string usernameValue = formData.username_field.value;
  std::u16string passwordValue = formData.password_field.value;

  // Don't fill if:
  // 1. Waiting for the user to type a username.
  // 2. |formData|'s origin is not matching the origin of the last commited URL.
  // 3. If a field has user typed input or input filled on user trigger.
  if (formData.wait_for_username ||
      formData.url.GetOrigin() != self.lastCommittedURL.GetOrigin() ||
      self.fieldDataManager->WasAutofilledOnUserTrigger(passwordID) ||
      self.fieldDataManager->DidUserType(passwordID)) {
    if (completionHandler) {
      completionHandler(NO);
    }
    return;
  }

  // Send JSON over to the web view.
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(mainFrame, formData, UTF16ToUTF8(usernameValue),
                         UTF16ToUTF8(passwordValue),
                         base::BindOnce(^(BOOL success) {
                           if (success) {
                             weakSelf.fieldDataManager->UpdateFieldDataMap(
                                 usernameID, usernameValue,
                                 FieldPropertiesFlags::kAutofilledOnPageLoad);
                             weakSelf.fieldDataManager->UpdateFieldDataMap(
                                 passwordID, passwordValue,
                                 FieldPropertiesFlags::kAutofilledOnPageLoad);
                           }
                           if (completionHandler) {
                             completionHandler(success);
                           }
                         }));
}

- (void)fillPasswordForm:(FormRendererId)formIdentifier
        newPasswordIdentifier:(FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:(FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler {
  web::WebFrame* mainFrame = web::GetMainFrame(_webState);
  if (!mainFrame) {
    return;
  }

  // Send JSON over to the web view.
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(
          mainFrame, formIdentifier, newPasswordIdentifier,
          confirmPasswordIdentifier, generatedPassword,
          base::BindOnce(
              ^(BOOL success) {
                if (success) {
                  weakSelf.fieldDataManager->UpdateFieldDataMap(
                      newPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                  weakSelf.fieldDataManager->UpdateFieldDataMap(
                      confirmPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                }
                if (completionHandler) {
                  completionHandler(success);
                }
              }));
}

- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                    triggeredOnField:(FieldRendererId)uniqueFieldID
                   completionHandler:
                       (nullable void (^)(BOOL))completionHandler {
  web::WebFrame* mainFrame = web::GetMainFrame(_webState);
  if (!mainFrame) {
    return;
  }

  // Necessary copy so the values can be used inside a block.
  FieldRendererId usernameID = fillData.username_element_id;
  FieldRendererId passwordID = fillData.password_element_id;
  std::u16string usernameValue = fillData.username_value;
  std::u16string passwordValue = fillData.password_value;

  // Do not fill the username if filling was triggered on a password field and
  // the username field has user typed input.
  BOOL fillUsername = uniqueFieldID == usernameID ||
                      !_fieldDataManager->DidUserType(usernameID);
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(
          mainFrame, fillData, fillUsername, UTF16ToUTF8(usernameValue),
          UTF16ToUTF8(passwordValue), base::BindOnce(^(BOOL success) {
            if (success) {
              weakSelf.fieldDataManager->UpdateFieldDataMap(
                  usernameID, usernameValue,
                  FieldPropertiesFlags::kAutofilledOnUserTrigger);
              weakSelf.fieldDataManager->UpdateFieldDataMap(
                  passwordID, passwordValue,
                  FieldPropertiesFlags::kAutofilledOnUserTrigger);
            }
            if (completionHandler) {
              completionHandler(success);
            }
          }));
}

// Finds the password form named |formName| and calls
// |completionHandler| with the populated |FormData| data structure. |found| is
// YES if the current form was found successfully, NO otherwise.
- (void)extractPasswordFormData:(FormRendererId)formIdentifier
              completionHandler:(void (^)(BOOL found, const FormData& form))
                                    completionHandler {
  DCHECK(completionHandler);

  if (!_webState) {
    return;
  }

  web::WebFrame* mainFrame = web::GetMainFrame(_webState);
  if (!mainFrame) {
    return;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    completionHandler(NO, FormData());
    return;
  }

  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->ExtractForm(
          mainFrame, formIdentifier, base::BindOnce(^(NSString* jsonString) {
            FormData formData;
            if (!JsonStringToFormData(jsonString, &formData, pageURL)) {
              completionHandler(NO, FormData());
              return;
            }

            completionHandler(YES, formData);
          }));
}

- (void)setUpForUniqueIDsWithInitialState:(uint32_t)nextAvailableID
                                  inFrame:(web::WebFrame*)frame {
  autofill::FormUtilJavaScriptFeature::GetInstance()
      ->SetUpForUniqueIDsWithInitialState(frame, nextAvailableID);
}

- (void)updateFieldDataOnUserInput:(autofill::FieldRendererId)field_id
                        inputValue:(NSString*)value {
  self.fieldDataManager->UpdateFieldDataMap(
      field_id, base::SysNSStringToUTF16(value),
      autofill::FieldPropertiesFlags::kUserTyped);
}

@end
