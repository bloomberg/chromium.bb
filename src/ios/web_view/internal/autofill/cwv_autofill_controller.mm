// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/ios/shared_password_controller.h"
#include "components/sync/driver/sync_service.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#include "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "net/base/mac/url_conversions.h"

using autofill::FormRendererId;
using autofill::FieldRendererId;

@implementation CWVAutofillController {
  // Bridge to observe the |webState|.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Autofill agent associated with |webState|.
  AutofillAgent* _autofillAgent;

  // Autofill client associated with |webState|.
  std::unique_ptr<autofill::WebViewAutofillClientIOS> _autofillClient;

  // The |webState| which this autofill controller should observe.
  web::WebState* _webState;

  // Handles password autofilling related logic.
  std::unique_ptr<password_manager::PasswordManager> _passwordManager;
  std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>
      _passwordManagerClient;
  std::unique_ptr<ios_web_view::WebViewPasswordManagerDriver>
      _passwordManagerDriver;
  SharedPasswordController* _passwordController;

  // The current credit card saver. Can be nil if no save attempt is pending.
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardSaver* _saver;

  // The current credit card verifier. Can be nil if no verification is pending.
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardVerifier* _verifier;

  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  std::string _lastFormActivityWebFrameID;
  NSString* _lastFormActivityTypedValue;
  NSString* _lastFormActivityType;
  FormRendererId _lastFormActivityUniqueFormID;
  FieldRendererId _lastFormActivityUniqueFieldID;
}

@synthesize delegate = _delegate;

- (instancetype)
         initWithWebState:(web::WebState*)webState
           autofillClient:(std::unique_ptr<autofill::WebViewAutofillClientIOS>)
                              autofillClient
            autofillAgent:(AutofillAgent*)autofillAgent
          passwordManager:(std::unique_ptr<password_manager::PasswordManager>)
                              passwordManager
    passwordManagerClient:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
            passwordManagerClient
    passwordManagerDriver:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerDriver>)
            passwordManagerDriver
       passwordController:(SharedPasswordController*)passwordController
        applicationLocale:(const std::string&)applicationLocale {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;

    _autofillAgent = autofillAgent;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());

    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);

    _autofillClient = std::move(autofillClient);
    _autofillClient->set_bridge(self);

    autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
        _webState, _autofillClient.get(), self, applicationLocale,
        autofill::BrowserAutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

    _passwordManagerClient = std::move(passwordManagerClient);
    _passwordManagerClient->set_bridge(self);
    _passwordManager = std::move(passwordManager);
    _passwordManagerDriver = std::move(passwordManagerDriver);
    _passwordManagerDriver->set_bridge(passwordController);
    _passwordController = passwordController;
    _passwordController.delegate = self;

    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(handlePasswordStoreSyncToggledNotification:)
               name:CWVPasswordStoreSyncToggledNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _formActivityObserverBridge.reset();
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
}

#pragma mark - Public Methods

- (void)clearFormWithName:(NSString*)formName
          fieldIdentifier:(NSString*)fieldIdentifier
                  frameID:(NSString*)frameID
        completionHandler:(nullable void (^)(void))completionHandler {
  web::WebFrame* frame =
      web::GetWebFrameWithId(_webState, base::SysNSStringToUTF8(frameID));
  autofill::AutofillJavaScriptFeature::GetInstance()
      ->ClearAutofilledFieldsForFormName(
          frame, formName, _lastFormActivityUniqueFormID, fieldIdentifier,
          _lastFormActivityUniqueFieldID, base::BindOnce(^(NSString*) {
            if (completionHandler) {
              completionHandler();
            }
          }));
}

- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                        fieldIdentifier:(NSString*)fieldIdentifier
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>* _Nonnull))
                              completionHandler {
  NSMutableArray<CWVAutofillSuggestion*>* allSuggestions =
      [NSMutableArray array];
  __block NSInteger pendingFetches = 0;
  void (^resultHandler)(NSArray<CWVAutofillSuggestion*>*) =
      ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
        [allSuggestions addObjectsFromArray:suggestions];
        if (pendingFetches == 0) {
          completionHandler(allSuggestions);
        }
      };

  // Construct query.
  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:formName
          uniqueFormID:_lastFormActivityUniqueFormID
       fieldIdentifier:fieldIdentifier
         uniqueFieldID:_lastFormActivityUniqueFieldID
             fieldType:fieldType
                  type:_lastFormActivityType
            typedValue:_lastFormActivityTypedValue
               frameID:frameID];
  // It is necessary to call |checkIfSuggestionsAvailableForForm| before
  // |retrieveSuggestionsForForm| because the former actually queries the db,
  // while the latter merely returns them.
  NSString* mainFrameID =
      base::SysUTF8ToNSString(web::GetMainWebFrameId(_webState));
  BOOL isMainFrame = [frameID isEqualToString:mainFrameID];

  // Check both autofill and password suggestions.
  NSArray<id<FormSuggestionProvider>>* providers =
      @[ _passwordController, _autofillAgent ];
  pendingFetches = providers.count;
  for (id<FormSuggestionProvider> suggestionProvider in providers) {
    __weak CWVAutofillController* weakSelf = self;
    id availableHandler = ^(BOOL suggestionsAvailable) {
      pendingFetches--;
      CWVAutofillController* strongSelf = weakSelf;
      if (!strongSelf) {
        resultHandler(@[]);
        return;
      }
      BOOL isPasswordSuggestion =
          suggestionProvider == strongSelf->_passwordController;
      id retrieveHandler =
          ^(NSArray* suggestions, id<FormSuggestionProvider> delegate) {
            NSMutableArray* autofillSuggestions = [NSMutableArray array];
            for (FormSuggestion* formSuggestion in suggestions) {
              CWVAutofillSuggestion* autofillSuggestion =
                  [[CWVAutofillSuggestion alloc]
                      initWithFormSuggestion:formSuggestion
                                    formName:formName
                             fieldIdentifier:fieldIdentifier
                                     frameID:frameID
                        isPasswordSuggestion:isPasswordSuggestion];
              [autofillSuggestions addObject:autofillSuggestion];
            }
            resultHandler([autofillSuggestions copy]);
          };
      [suggestionProvider retrieveSuggestionsForForm:formQuery
                                            webState:strongSelf->_webState
                                   completionHandler:retrieveHandler];
    };

    [suggestionProvider checkIfSuggestionsAvailableForForm:formQuery
                                               isMainFrame:isMainFrame
                                            hasUserGesture:YES
                                                  webState:_webState
                                         completionHandler:availableHandler];
  }
}

- (void)acceptSuggestion:(CWVAutofillSuggestion*)suggestion
       completionHandler:(nullable void (^)(void))completionHandler {
  if (suggestion.isPasswordSuggestion) {
    [_passwordController didSelectSuggestion:suggestion.formSuggestion
                                        form:suggestion.formName
                                uniqueFormID:_lastFormActivityUniqueFormID
                             fieldIdentifier:suggestion.fieldIdentifier
                               uniqueFieldID:_lastFormActivityUniqueFieldID
                                     frameID:suggestion.frameID
                           completionHandler:^{
                             if (completionHandler) {
                               completionHandler();
                             }
                           }];
  } else {
    [_autofillAgent didSelectSuggestion:suggestion.formSuggestion
                                   form:suggestion.formName
                           uniqueFormID:_lastFormActivityUniqueFormID
                        fieldIdentifier:suggestion.fieldIdentifier
                          uniqueFieldID:_lastFormActivityUniqueFieldID
                                frameID:suggestion.frameID
                      completionHandler:^{
                        if (completionHandler) {
                          completionHandler();
                        }
                      }];
  }
}

- (void)focusPreviousField {
  web::WebFrame* frame = _webState->GetWebFramesManager()->GetFrameWithId(
      _lastFormActivityWebFrameID);

  if (!frame)
    return;

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectPreviousElementInFrame(frame);
}

- (void)focusNextField {
  web::WebFrame* frame = _webState->GetWebFramesManager()->GetFrameWithId(
      _lastFormActivityWebFrameID);

  if (!frame)
    return;

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectNextElementInFrame(frame);
}

- (void)checkIfPreviousAndNextFieldsAreAvailableForFocusWithCompletionHandler:
    (void (^)(BOOL previous, BOOL next))completionHandler {
  web::WebFrame* frame = _webState->GetWebFramesManager()->GetFrameWithId(
      _lastFormActivityWebFrameID);

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->FetchPreviousAndNextElementsPresenceInFrame(
          frame, base::BindOnce(completionHandler));
}

#pragma mark - CWVAutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                    delegate {
  // frontend_id is > 0 for Autofill suggestions, == 0 for Autocomplete
  // suggestions, and < 0 for special suggestions such as clear form.
  // We only want Autofill suggestions.
  std::vector<autofill::Suggestion> filtered_suggestions;
  std::copy_if(suggestions.begin(), suggestions.end(),
               std::back_inserter(filtered_suggestions),
               [](autofill::Suggestion suggestion) {
                 return suggestion.frontend_id > 0;
               });
  [_autofillAgent showAutofillPopup:filtered_suggestions
                      popupDelegate:delegate];
}

- (void)hideAutofillPopup {
  [_autofillAgent hideAutofillPopup];
}

- (bool)isQueryIDRelevant:(int)queryID {
  return [_autofillAgent isQueryIDRelevant:queryID];
}

- (void)
    confirmSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
               legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
           saveCreditCardOptions:
               (autofill::AutofillClient::SaveCreditCardOptions)
                   saveCreditCardOptions
                        callback:(autofill::AutofillClient::
                                      UploadSaveCardPromptCallback)callback {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                          saveCreditCardWithSaver:)]) {
    return;
  }
  CWVCreditCardSaver* saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:creditCard
                                         saveOptions:saveCreditCardOptions
                                   legalMessageLines:legalMessageLines
                                  savePromptCallback:std::move(callback)];
  [_delegate autofillController:self saveCreditCardWithSaver:saver];
  _saver = saver;
}

- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved {
  [_saver handleCreditCardUploadCompleted:cardSaved];
}

- (void)
showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
                 reason:(autofill::AutofillClient::UnmaskCardReason)reason
               delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)delegate {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:verifyCreditCardWithVerifier:)]) {
    ios_web_view::WebViewBrowserState* browserState =
        ios_web_view::WebViewBrowserState::FromBrowserState(
            _webState->GetBrowserState());
    CWVCreditCardVerifier* verifier = [[CWVCreditCardVerifier alloc]
         initWithPrefs:browserState->GetPrefs()
        isOffTheRecord:browserState->IsOffTheRecord()
            creditCard:creditCard
                reason:reason
              delegate:delegate];
    [_delegate autofillController:self verifyCreditCardWithVerifier:verifier];

    // Store so verifier can receive unmask verification results later on.
    _verifier = verifier;
  }
}

- (void)didReceiveUnmaskVerificationResult:
    (autofill::AutofillClient::PaymentsRpcResult)result {
  [_verifier didReceiveUnmaskVerificationResult:result];
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  if (_verifier) {
    [_verifier loadRiskData:std::move(callback)];
  } else if (_saver) {
    [_saver loadRiskData:std::move(callback)];
  }
}

- (void)propagateAutofillPredictionsForForms:
    (const std::vector<autofill::FormStructure*>&)forms {
  _passwordManager->ProcessAutofillPredictions(_passwordManagerDriver.get(),
                                               forms);
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillFormData:(const autofill::FormData&)form
             inFrame:(web::WebFrame*)frame {
  [_autofillAgent fillFormData:form inFrame:frame];
}

- (void)handleParsedForms:(const std::vector<autofill::FormStructure*>&)forms
                  inFrame:(web::WebFrame*)frame {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                                     didFindForms:frameID:)]) {
    return;
  }

  NSMutableArray<CWVAutofillForm*>* autofillForms = [NSMutableArray array];
  for (autofill::FormStructure* form : forms) {
    CWVAutofillForm* autofillForm =
        [[CWVAutofillForm alloc] initWithFormStructure:*form];
    [autofillForms addObject:autofillForm];
  }
  [_delegate autofillController:self
                   didFindForms:autofillForms
                        frameID:base::SysUTF8ToNSString(frame->GetFrameId())];
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  // Not supported.
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  NSString* nsFormName = base::SysUTF8ToNSString(params.form_name);
  _lastFormActivityUniqueFormID = params.unique_form_id;
  NSString* nsFieldIdentifier =
      base::SysUTF8ToNSString(params.field_identifier);
  _lastFormActivityUniqueFieldID = params.unique_field_id;
  NSString* nsFieldType = base::SysUTF8ToNSString(params.field_type);
  NSString* nsFrameID = base::SysUTF8ToNSString(GetWebFrameId(frame));
  NSString* nsValue = base::SysUTF8ToNSString(params.value);
  NSString* nsType = base::SysUTF8ToNSString(params.type);
  BOOL userInitiated = params.has_user_gesture;

  _lastFormActivityWebFrameID = GetWebFrameId(frame);
  _lastFormActivityTypedValue = nsValue;
  _lastFormActivityType = nsType;
  if (params.type == "focus") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didFocusOnFieldWithIdentifier:fieldType:formName:frameID
                                                    :value:userInitiated:)]) {
      [_delegate autofillController:self
          didFocusOnFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue
                          userInitiated:userInitiated];
    }
  } else if (params.type == "input") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didInputInFieldWithIdentifier:fieldType:formName:frameID
                                                    :value:userInitiated:)]) {
      [_delegate autofillController:self
          didInputInFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue
                          userInitiated:userInitiated];
    }
  } else if (params.type == "blur") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didBlurOnFieldWithIdentifier:fieldType:formName:frameID
                                                   :value:userInitiated:)]) {
      [_delegate autofillController:self
          didBlurOnFieldWithIdentifier:nsFieldIdentifier
                             fieldType:nsFieldType
                              formName:nsFormName
                               frameID:nsFrameID
                                 value:nsValue
                         userInitiated:userInitiated];
    }
  }
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)userInitiated
                   formInMainFrame:(BOOL)isMainFrame
                           inFrame:(web::WebFrame*)frame {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:
                     didSubmitFormWithName:frameID:userInitiated:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF8ToNSString(formName)
                          frameID:base::SysUTF8ToNSString(frame->GetFrameId())
                    userInitiated:userInitiated];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _formActivityObserverBridge.reset();
  _autofillClient.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
  _webState = nullptr;
}

#pragma mark - PasswordManagerClientBridge

- (web::WebState*)webState {
  return _webState;
}

- (password_manager::PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)showSavePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToSave
                         manual:(BOOL)manual {
  if (![self.delegate respondsToSelector:@selector
                      (autofillController:
                          decideSavePolicyForPassword:decisionHandler:)]) {
    return;
  }

  __block std::unique_ptr<password_manager::PasswordFormManagerForUI> formPtr(
      std::move(formToSave));

  const password_manager::PasswordForm& credentials =
      formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  [self.delegate autofillController:self
        decideSavePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      switch (decision) {
                        case CWVPasswordUserDecisionYes:
                          formPtr->Save();
                          break;
                        case CWVPasswordUserDecisionNever:
                          formPtr->Blocklist();
                          break;
                        case CWVPasswordUserDecisionNotThisTime:
                          // Do nothing.
                          break;
                      }
                    }];
}

- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToUpdate
                           manual:(BOOL)manual {
  if (![self.delegate respondsToSelector:@selector
                      (autofillController:
                          decideUpdatePolicyForPassword:decisionHandler:)]) {
    return;
  }

  __block std::unique_ptr<password_manager::PasswordFormManagerForUI> formPtr(
      std::move(formToUpdate));

  const password_manager::PasswordForm& credentials =
      formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  [self.delegate autofillController:self
      decideUpdatePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      // Marking a password update as "never" makes no sense as
                      // the password has already been saved.
                      DCHECK_NE(decision, CWVPasswordUserDecisionNever)
                          << "A password update can only be accepted or "
                             "ignored.";
                      if (decision == CWVPasswordUserDecisionYes) {
                        formPtr->Update(credentials);
                      }
                    }];
}

- (void)removePasswordInfoBarManualFallback:(BOOL)manual {
  // No op.
}

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL {
  if ([self.delegate
          respondsToSelector:@selector(autofillController:
                                 notifyUserOfPasswordLeakOnURL:leakType:)]) {
    CWVPasswordLeakType cwvLeakType = 0;
    if (password_manager::IsPasswordSaved(leakType)) {
      cwvLeakType |= CWVPasswordLeakTypeSaved;
    }
    if (password_manager::IsPasswordUsedOnOtherSites(leakType)) {
      cwvLeakType |= CWVPasswordLeakTypeUsedOnOtherSites;
    }
    if (password_manager::IsSyncingPasswordsNormally(leakType)) {
      cwvLeakType |= CWVPasswordLeakTypeSyncingNormally;
    }
    [self.delegate autofillController:self
        notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(URL)
                             leakType:cwvLeakType];
  }
}

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  // No op.
}

#pragma mark - SharedPasswordControllerDelegate

- (password_manager::PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (password_manager::PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                   decisionHandler:(void (^)(BOOL accept))decisionHandler {
  if ([self.delegate
          respondsToSelector:@selector(autofillController:
                                 suggestGeneratedPassword:decisionHandler:)]) {
    [self.delegate autofillController:self
             suggestGeneratedPassword:generatedPotentialPassword
                      decisionHandler:decisionHandler];
  } else {
    // If not implemented, just reject.
    decisionHandler(/*accept=*/NO);
  }
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion {
  // No op.
}

#pragma mark - Private Methods

- (void)handlePasswordStoreSyncToggledNotification:
    (NSNotification*)notification {
  NSValue* wrappedBrowserState =
      notification.userInfo[CWVPasswordStoreNotificationBrowserStateKey];
  ios_web_view::WebViewBrowserState* browserState =
      static_cast<ios_web_view::WebViewBrowserState*>(
          wrappedBrowserState.pointerValue);
  if (_webState->GetBrowserState() == browserState) {
    _passwordManagerClient->UpdateFormManagers();
  }
}

@end
