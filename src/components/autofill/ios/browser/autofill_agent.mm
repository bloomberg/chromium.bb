// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using FormDataVector = std::vector<autofill::FormData>;

// The type of the completion handler block for
// |fetchFormsWithName:minimumRequiredFieldsCount:completionHandler|
typedef void (^FetchFormsCompletionHandler)(BOOL, const FormDataVector&);

// Gets the field specified by |fieldIdentifier| from |form|, if focusable. Also
// modifies the field's value for the select elements.
void GetFormField(autofill::FormFieldData* field,
                  const autofill::FormData& form,
                  const base::string16& fieldIdentifier) {
  for (const auto& currentField : form.fields) {
    if (currentField.unique_id == fieldIdentifier &&
        currentField.is_focusable) {
      *field = currentField;
      break;
    }
  }
  if (field->SameFieldAs(autofill::FormFieldData()))
    return;

  // Hack to get suggestions from select input elements.
  if (field->form_control_type == "select-one") {
    // Any value set will cause the AutofillManager to filter suggestions (only
    // show suggestions that begin the same as the current value) with the
    // effect that one only suggestion would be returned; the value itself.
    field->value = base::string16();
  }
}

}  // namespace

@interface AutofillAgent () <CRWWebStateObserver,
                             FormActivityObserver,
                             PrefObserverDelegate> {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* webState_;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // The pref service for which this agent was created.
  PrefService* prefService_;

  // Manager for Autofill JavaScripts.
  JsAutofillManager* jsAutofillManager_;

  // The name of the most recent autocomplete field; tracks the currently-
  // focused form element in order to force filling of the currently selected
  // form element, even if it's non-empty.
  base::string16 pendingAutocompleteField_;

  // Suggestions state:
  // The most recent form suggestions.
  NSArray* mostRecentSuggestions_;

  // The completion to inform FormSuggestionController that a user selection
  // has been handled.
  SuggestionHandledCompletion suggestionHandledCompletion_;

  // The completion to inform FormSuggestionController that suggestions are
  // available for a given form and field.
  SuggestionsAvailableCompletion suggestionsAvailableCompletion_;

  // The text entered by the user into the active field.
  NSString* typedValue_;

  // Popup delegate for the most recent suggestions.
  // The reference is weak because a weak pointer is sent to our
  // AutofillManagerDelegate.
  base::WeakPtr<autofill::AutofillPopupDelegate> popupDelegate_;

  // The autofill data that needs to be send when the |webState_| is shown.
  // The pair contains the frame ID and the base::Value to send.
  // If the value is nullptr, no data needs to be sent.
  std::pair<std::string, std::unique_ptr<base::Value>> pendingFormData_;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> prefObserverBridge_;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar prefChangeRegistrar_;

  // Bridge to observe form activity in |webState_|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      formActivityObserverBridge_;

  // AutofillDriverIOSWebFrame will keep a refcountable AutofillDriverIOS.
  // This is a workaround crbug.com/892612. On submission,
  // AutofillDownloadManager and CreditCardSaveManager expect AutofillManager
  // and AutofillDriver to live after web frame deletion so AutofillAgent will
  // keep the latest submitted AutofillDriver alive.
  // TODO(crbug.com/892612): remove this workaround once life cycle of
  // AutofillManager is fixed.
  scoped_refptr<autofill::AutofillDriverIOSRefCountable>
      last_submitted_autofill_driver_;
}

@end

@implementation AutofillAgent

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState {
  DCHECK(prefService);
  DCHECK(webState);
  self = [super init];
  if (self) {
    webState_ = webState;
    webStateObserverBridge_ =
        std::make_unique<web::WebStateObserverBridge>(self);
    webState_->AddObserver(webStateObserverBridge_.get());
    formActivityObserverBridge_ =
        std::make_unique<autofill::FormActivityObserverBridge>(webState_, self);
    prefService_ = prefService;
    prefObserverBridge_ = std::make_unique<PrefObserverBridge>(self);
    prefChangeRegistrar_.Init(prefService);
    prefObserverBridge_->ObserveChangesForPreference(
        autofill::prefs::kAutofillCreditCardEnabled, &prefChangeRegistrar_);
    prefObserverBridge_->ObserveChangesForPreference(
        autofill::prefs::kAutofillProfileEnabled, &prefChangeRegistrar_);

    jsAutofillManager_ = [[JsAutofillManager alloc]
        initWithReceiver:webState_->GetJSInjectionReceiver()];
  }
  return self;
}

- (void)dealloc {
  if (webState_) {
    [self webStateDestroyed:webState_];
  }
}

#pragma mark - Private methods

// Returns the autofill manager associated with a web::WebState instance.
// Returns nullptr if there is no autofill manager associated anymore, this can
// happen when |close| has been called on the |webState|. Also returns nullptr
// if -webStateDestroyed: has been called.
- (autofill::AutofillManager*)
autofillManagerFromWebState:(web::WebState*)webState
                   webFrame:(web::WebFrame*)webFrame {
  if (!webState || !webStateObserverBridge_)
    return nullptr;
  return autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState,
                                                              webFrame)
      ->autofill_manager();
}

// Notifies the autofill manager when forms are detected on a page.
- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
                  ofFormsSeen:(const FormDataVector&)forms {
  DCHECK(autofillManager);
  DCHECK(!forms.empty());
  autofillManager->OnFormsSeen(forms, base::TimeTicks::Now());
}

// Notifies the autofill manager when forms are submitted.
- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
             ofFormsSubmitted:(const FormDataVector&)forms
                userInitiated:(BOOL)userInitiated {
  DCHECK(autofillManager);
  // Exactly one form should be extracted.
  DCHECK_EQ(1U, forms.size());
  autofill::FormData form = forms[0];
  autofillManager->OnFormSubmitted(form, false,
                                   autofill::SubmissionSource::FORM_SUBMISSION);
  autofill::KeyboardAccessoryMetricsLogger::OnFormSubmitted();
}

// Invokes the form extraction script in |frame| and loads the output into the
// format expected by the AutofillManager.
// If |filtered| is NO, all forms are extracted.
// If |filtered| is YES,
//   - if |formName| is non-empty, only a form of that name is extracted.
//   - if |formName| is empty, unowned fields are extracted.
// Only forms with at least |requiredFieldsCount| fields are extracted.
// Calls |completionHandler| with a success BOOL of YES and the form data that
// was extracted.
// Calls |completionHandler| with NO if the forms could not be extracted.
// |completionHandler| cannot be nil.
- (void)fetchFormsFiltered:(BOOL)filtered
                      withName:(const base::string16&)formName
    minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                       inFrame:(web::WebFrame*)frame
             completionHandler:(FetchFormsCompletionHandler)completionHandler {
  DCHECK(completionHandler);

  // Necessary so the values can be used inside a block.
  base::string16 formNameCopy = formName;
  GURL pageURL = webState_->GetLastCommittedURL();
  GURL frameOrigin = frame ? frame->GetSecurityOrigin() : pageURL.GetOrigin();
  [jsAutofillManager_
      fetchFormsWithMinimumRequiredFieldsCount:requiredFieldsCount
                                       inFrame:frame
                             completionHandler:^(NSString* formJSON) {
                               std::vector<autofill::FormData> formData;
                               bool success = autofill::ExtractFormsData(
                                   formJSON, filtered, formNameCopy, pageURL,
                                   frameOrigin, &formData);
                               completionHandler(success, formData);
                             }];
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
             popupDelegate:
                 (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                     delegate {
  popupDelegate_ = delegate;
  mostRecentSuggestions_ = suggestions;
  if (suggestionsAvailableCompletion_)
    suggestionsAvailableCompletion_([mostRecentSuggestions_ count] > 0);
  suggestionsAvailableCompletion_ = nil;
}

#pragma mark - FormSuggestionProvider

// Sends a request to AutofillManager to retrieve suggestions for the specified
// form and field.
- (void)queryAutofillForForm:(const autofill::FormData&)form
             fieldIdentifier:(NSString*)fieldIdentifier
                        type:(NSString*)type
                  typedValue:(NSString*)typedValue
                     frameID:(NSString*)frameID
                    webState:(web::WebState*)webState
           completionHandler:(SuggestionsAvailableCompletion)completion {
  web::WebFrame* frame =
      GetWebFrameWithId(webState, base::SysNSStringToUTF8(frameID));
  autofill::AutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState webFrame:frame];
  if (!autofillManager)
    return;

  // Passed to delegates; we don't use it so it's set to zero.
  int queryId = 0;

  // Find the right field.
  autofill::FormFieldData field;
  GetFormField(&field, form, base::SysNSStringToUTF16(fieldIdentifier));

  // Save the completion and go look for suggestions.
  suggestionsAvailableCompletion_ = [completion copy];
  typedValue_ = [typedValue copy];

  // Query the AutofillManager for suggestions. Results will arrive in
  // -showAutofillPopup:popupDelegate:.
  autofillManager->OnQueryFormFieldAutofill(
      queryId, form, field, gfx::RectF(),
      /*autoselect_first_suggestion=*/false);
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  DCHECK_EQ(webState_, webState);

  if (![self isAutofillEnabled]) {
    completion(NO);
    return;
  }

  // Check for suggestions if the form activity is initiated by the user.
  if (!hasUserGesture) {
    completion(NO);
    return;
  }

  web::WebFrame* frame =
      web::GetWebFrameWithId(webState_, base::SysNSStringToUTF8(frameID));
  if (!frame && autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    completion(NO);
    return;
  }

  // Once the active form and field are extracted, send a query to the
  // AutofillManager for suggestions.
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    if (success && forms.size() == 1) {
      [weakSelf queryAutofillForForm:forms[0]
                     fieldIdentifier:fieldIdentifier
                                type:type
                          typedValue:typedValue
                             frameID:frameID
                            webState:webState
                   completionHandler:completion];
    }
  };

  // Re-extract the active form and field only. All forms with at least one
  // input element are considered because key/value suggestions are offered
  // even on short forms.
  [self fetchFormsFiltered:YES
                        withName:base::SysNSStringToUTF16(formName)
      minimumRequiredFieldsCount:1
                         inFrame:frame
               completionHandler:completionHandler];
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(mostRecentSuggestions_)
      << "Requestor should have called "
      << "|checkIfSuggestionsAvailableForForm:fieldIdentifier:fieldType:type:"
      << "typedValue:frameID:isMainFrame:hasUserGesture:webState:"
      << "completionHandler:| and waited for the result before calling "
      << "|retrieveSuggestionsForForm:fieldIdentifier:fieldType:type:"
      << "typedValue:frameID:webState:completionHandler:|.";
  completion(mostRecentSuggestions_, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  [[UIDevice currentDevice] playInputClick];
  suggestionHandledCompletion_ = [completion copy];

  if (suggestion.identifier > 0) {
    pendingAutocompleteField_ = base::SysNSStringToUTF16(fieldIdentifier);
    if (popupDelegate_) {
      popupDelegate_->DidAcceptSuggestion(
          base::SysNSStringToUTF16(suggestion.value), suggestion.identifier, 0);
    }
  } else if (suggestion.identifier ==
             autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    web::WebFrame* frame =
        web::GetWebFrameWithId(webState_, base::SysNSStringToUTF8(frameID));
    // FormSuggestion is a simple, single value that can be filled out now.
    [self fillField:base::SysNSStringToUTF8(fieldIdentifier)
           formName:base::SysNSStringToUTF8(formName)
              value:base::SysNSStringToUTF16(suggestion.value)
            inFrame:frame];
  } else if (suggestion.identifier == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
    web::WebFrame* frame =
        web::GetWebFrameWithId(webState_, base::SysNSStringToUTF8(frameID));
    [jsAutofillManager_
        clearAutofilledFieldsForFormName:formName
                         fieldIdentifier:fieldIdentifier
                                 inFrame:frame
                       completionHandler:suggestionHandledCompletion_];
    suggestionHandledCompletion_ = nil;
  } else {
    NOTREACHED() << "unknown identifier " << suggestion.identifier;
  }
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillFormData:(const autofill::FormData&)form
             inFrame:(web::WebFrame*)frame {
  auto autofillData =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  autofillData->SetKey("formName", base::Value(base::UTF16ToUTF8(form.name)));

  base::Value fieldsData(base::Value::Type::DICTIONARY);
  for (const auto& field : form.fields) {
    // Skip empty fields and those that are not autofilled.
    if (field.value.empty() || !field.is_autofilled)
      continue;

    base::Value fieldData(base::Value::Type::DICTIONARY);
    fieldData.SetKey("value", base::Value(field.value));
    fieldData.SetKey("section", base::Value(field.section));
    fieldsData.SetKey(base::UTF16ToUTF8(field.unique_id), std::move(fieldData));
  }
  autofillData->SetKey("fields", std::move(fieldsData));

  // Store the form data when WebState is not visible, to send it as soon as it
  // becomes visible again, e.g., when the CVC unmask prompt is showing.
  if (!webState_->IsVisible()) {
    pendingFormData_ =
        std::make_pair(web::GetWebFrameId(frame), std::move(autofillData));
  } else {
    [self sendData:std::move(autofillData) toFrame:frame];
  }

  autofill::AutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState_ webFrame:frame];
  if (autofillManager)
    autofillManager->OnDidFillAutofillFormData(form, base::TimeTicks::Now());
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowTypePredictions)) {
    return;
  }

  auto predictionData =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  for (const auto& form : forms) {
    base::Value fieldData(base::Value::Type::DICTIONARY);
    DCHECK(form.fields.size() == form.data.fields.size());
    for (size_t i = 0; i < form.fields.size(); i++) {
      fieldData.SetKey(base::UTF16ToUTF8(form.data.fields[i].unique_id),
                       base::Value(form.fields[i].overall_type));
    }
    predictionData->SetKey(base::UTF16ToUTF8(form.data.name),
                           std::move(fieldData));
  }
  [jsAutofillManager_ fillPredictionData:std::move(predictionData)
                                 inFrame:frame];
}

#pragma mark - AutofillClientIOSBridge

- (void)showAutofillPopup:
            (const std::vector<autofill::Suggestion>&)popup_suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                    delegate {
  bool has_gpay_branding = false;
  // Convert the suggestions into an NSArray for the keyboard.
  NSMutableArray<FormSuggestion*>* suggestions = [[NSMutableArray alloc] init];
  for (auto popup_suggestion : popup_suggestions) {
    // In the Chromium implementation the identifiers represent rows on the
    // drop down of options. These include elements that aren't relevant to us
    // such as separators ... see blink::WebAutofillClient::MenuItemIDSeparator
    // for example. We can't include that enum because it's from WebKit, but
    // fortunately almost all the entries we are interested in (profile or
    // autofill entries) are zero or positive. Negative entries we are
    // interested in is autofill::POPUP_ITEM_ID_CLEAR_FORM, used to show the
    // "clear form" button and autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING, used
    // to show the "Google Pay" branding.
    NSString* value = nil;
    NSString* displayDescription = nil;
    if (popup_suggestion.frontend_id >= 0) {
      // Filter out any key/value suggestions if the user hasn't typed yet.
      if (popup_suggestion.frontend_id ==
              autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY &&
          [typedValue_ length] == 0) {
        continue;
      }
      // Value will contain the text to be filled in the selected element while
      // displayDescription will contain a summary of the data to be filled in
      // the other elements.
      value = base::SysUTF16ToNSString(popup_suggestion.value);
      displayDescription = base::SysUTF16ToNSString(popup_suggestion.label);
    } else if (popup_suggestion.frontend_id ==
               autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      // Show the "clear form" button.
      value = base::SysUTF16ToNSString(popup_suggestion.value);
    } else if (popup_suggestion.frontend_id ==
               autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING) {
      // Show "GPay branding" icon
      value = base::SysUTF16ToNSString(popup_suggestion.value);
      has_gpay_branding = true;
    }

    if (!value)
      continue;

    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:value
         displayDescription:displayDescription
                       icon:base::SysUTF8ToNSString(popup_suggestion.icon)
                 identifier:popup_suggestion.frontend_id];

    // Put "clear form" entry at the front of the suggestions. If
    // "GPay branding" icon is present, it remains as the first suggestion.
    if (popup_suggestion.frontend_id == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      [suggestions insertObject:suggestion atIndex:has_gpay_branding ? 1 : 0];
    } else {
      [suggestions addObject:suggestion];
    }
  }

  [self onSuggestionsReady:suggestions popupDelegate:delegate];

  if (delegate)
    delegate->OnPopupShown();
}

- (void)hideAutofillPopup {
  [self onSuggestionsReady:@[]
             popupDelegate:base::WeakPtr<autofill::AutofillPopupDelegate>()];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  if (pendingFormData_.second) {
    // If IsAutofillIFrameMessagingEnabled, the frameID cannot be empty.
    DCHECK(!autofill::switches::IsAutofillIFrameMessagingEnabled() ||
           !pendingFormData_.first.empty());
    web::WebFrame* frame = nullptr;
    if (!pendingFormData_.first.empty()) {
      frame = web::GetWebFrameWithId(webState_, pendingFormData_.first);
    }
    [self sendData:std::move(pendingFormData_.second) toFrame:frame];
  }
  pendingFormData_ = std::make_pair("", nullptr);
}

- (void)webState:(web::WebState*)webState
    frameDidBecomeAvailable:(web::WebFrame*)web_frame {
  DCHECK_EQ(webState_, webState);
  DCHECK(web_frame);
  if (![self isAutofillEnabled] || webState->IsLoading()) {
    return;
  }
  if (web_frame->IsMainFrame()) {
    [self processPage:webState];
    return;
  }
  if (!autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    // iFrame support is disabled.
    return;
  }
  // Check that the main frame has already been processed.
  if (!web::GetMainWebFrame(webState)) {
    return;
  }
  if (!autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
           webState, web::GetMainWebFrame(webState))
           ->is_processed()) {
    return;
  }
  [self processFrame:web_frame inWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(webState_, webState);
  // Ignore navigations within the same document, e.g., history.pushState().
  if (navigation->IsSameDocument())
    return;
  if (!autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    // Reset AutofillManager before processing the new page.
    web::WebFrame* frame = web::GetMainWebFrame(webState);
    autofill::AutofillManager* autofillManager =
        [self autofillManagerFromWebState:webState webFrame:frame];
    DCHECK(autofillManager);
    autofillManager->Reset();
    autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, nullptr)
        ->set_processed(false);
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(webState_, webState);
  if (![self isAutofillEnabled])
    return;

  [self processPage:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  last_submitted_autofill_driver_ = nullptr;
  if (webState_) {
    formActivityObserverBridge_.reset();
    webState_->RemoveObserver(webStateObserverBridge_.get());
    webStateObserverBridge_.reset();
    webState_ = nullptr;
  }
  // Do not wait for deallocation. Remove all observers here.
  prefChangeRegistrar_.RemoveAll();
}

#pragma mark - Private methods

- (void)processPage:(web::WebState*)webState {
  web::WebFramesManager* framesManager =
      web::WebFramesManager::FromWebState(webState);
  DCHECK(framesManager);
  if (!framesManager->GetMainWebFrame()) {
    return;
  }
  [self processFrame:framesManager->GetMainWebFrame() inWebState:webState];
  if (autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    [self processFrame:framesManager->GetMainWebFrame() inWebState:webState];
    for (auto* frame : framesManager->GetAllWebFrames()) {
      if (frame->IsMainFrame()) {
        continue;
      }
      [self processFrame:frame inWebState:webState];
    }
  }
}

- (void)processFrame:(web::WebFrame*)frame inWebState:(web::WebState*)webState {
  if (!frame || !frame->CanCallJavaScriptFunction())
    return;

  autofill::AutofillDriverIOS* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  // This process is only done once.
  if (driver->is_processed())
    return;
  driver->set_processed(true);
  [jsAutofillManager_ addJSDelayInFrame:frame];

  if (frame->IsMainFrame()) {
    popupDelegate_.reset();
    suggestionsAvailableCompletion_ = nil;
    suggestionHandledCompletion_ = nil;
    mostRecentSuggestions_ = nil;
    typedValue_ = nil;
  }

  [jsAutofillManager_ toggleTrackingFormMutations:YES inFrame:frame];

  [jsAutofillManager_ toggleTrackingUserEditedFields:
                          base::FeatureList::IsEnabled(
                              autofill::features::kAutofillPrefilledFields)
                                             inFrame:frame];

  [self scanFormsInWebState:webState inFrame:frame];
}

- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    AutofillAgent* strongSelf = weakSelf;
    if (!strongSelf || !success)
      return;
    autofill::AutofillManager* autofillManager =
        [strongSelf autofillManagerFromWebState:webState webFrame:webFrame];
    if (!autofillManager || forms.empty())
      return;
    [strongSelf notifyAutofillManager:autofillManager ofFormsSeen:forms];
  };
  // The document has now been fully loaded. Scan for forms to be extracted.
  size_t min_required_fields =
      MIN(autofill::MinRequiredFieldsForUpload(),
          MIN(autofill::MinRequiredFieldsForHeuristics(),
              autofill::MinRequiredFieldsForQuery()));
  [self fetchFormsFiltered:NO
                        withName:base::string16()
      minimumRequiredFieldsCount:min_required_fields
                         inFrame:webFrame
               completionHandler:completionHandler];
}

#pragma mark -
#pragma mark FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  if (![self isAutofillEnabled])
    return;

  if (!frame || !frame->CanCallJavaScriptFunction())
    return;

  // Return early if the page is not processed yet.
  DCHECK(autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame));
  if (!autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame)
           ->is_processed())
    return;

  // Return early if |params| is not complete.
  if (params.input_missing)
    return;

  // If the event is a form_changed, then the event concerns the whole page and
  // not a particular form. The whole page need to be reparsed to find the new
  // forms.
  if (params.type == "form_changed") {
    [self scanFormsInWebState:webState inFrame:frame];
    return;
  }

  // We are only interested in 'input' events in order to notify the autofill
  // manager for metrics purposes.
  if (params.type != "input" ||
      (params.field_type != "text" && params.field_type != "password")) {
    return;
  }

  // Necessary so the string can be used inside the block.
  std::string fieldIdentifier = params.field_identifier;

  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    if (!success || forms.size() != 1)
      return;

    DCHECK_EQ(webState_, webState);
    autofill::AutofillManager* autofillManager =
        [weakSelf autofillManagerFromWebState:webState webFrame:frame];
    if (!autofillManager)
      return;

    autofill::FormFieldData field;
    GetFormField(&field, forms[0], base::UTF8ToUTF16(fieldIdentifier));
    autofillManager->OnTextFieldDidChange(forms[0], field, gfx::RectF(),
                                          base::TimeTicks::Now());
  };

  // Extract the active form and field only. There is no minimum field
  // requirement because key/value suggestions are offered even on short forms.
  [self fetchFormsFiltered:YES
                        withName:base::UTF8ToUTF16(params.form_name)
      minimumRequiredFieldsCount:1
                         inFrame:frame
               completionHandler:completionHandler];
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                   formInMainFrame:(BOOL)formInMainFrame
                           inFrame:(web::WebFrame*)frame {
  if (![self isAutofillEnabled])
    return;
  if (!frame || !frame->CanCallJavaScriptFunction())
    return;
  FormDataVector forms;

  bool success = autofill::ExtractFormsData(
      base::SysUTF8ToNSString(formData), true, base::UTF8ToUTF16(formName),
      webState->GetLastCommittedURL(), frame->GetSecurityOrigin(), &forms);

  autofill::AutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState webFrame:frame];
  if (!autofillManager || !success || forms.empty())
    return;
  if (autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    // AutofillDriverIOSWebFrame will keep a refcountable AutofillDriverIOS.
    // This is a workaround crbug.com/892612. On submission,
    // AutofillDownloadManager and CreditCardSaveManager expect AutofillManager
    // and AutofillDriver to live after web frame deletion so AutofillAgent will
    // keep the latest submitted AutofillDriver alive.
    // TODO(crbug.com/892612): remove this workaround once life cycle of
    // AutofillManager is fixed.
    DCHECK(frame);
    last_submitted_autofill_driver_ =
        autofill::AutofillDriverIOSWebFrame::FromWebFrame(frame)
            ->GetRetainableDriver();
    DCHECK(last_submitted_autofill_driver_);
  }
  DCHECK(forms.size() <= 1) << "Only one form should be extracted.";
  [self notifyAutofillManager:autofillManager
             ofFormsSubmitted:forms
                userInitiated:hasUserGesture];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Processing the page can be needed here if Autofill is enabled in settings
  // when the page is already loaded.
  if ([self isAutofillEnabled])
    [self processPage:webState_];
}

#pragma mark - Private methods

// Returns whether Autofill is enabled by checking if Autofill is turned on and
// if the current URL has a web scheme and the page content is HTML.
- (BOOL)isAutofillEnabled {
  if (!autofill::prefs::IsAutofillEnabled(prefService_))
    return NO;

  // Only web URLs are supported by Autofill.
  return web::UrlHasWebScheme(webState_->GetLastCommittedURL()) &&
         webState_->ContentIsHTML();
}

// Complete a field identified with |fieldIdentifier| on the form named
// |formName| in |frame| using |value| then move the cursor.
// TODO(crbug.com/661621): |dataString| ends up at fillFormField() in
// autofill_controller.js. fillFormField() expects an AutofillFormFieldData
// object, which |dataString| is not because 'form' is not a specified member of
// AutofillFormFieldData. fillFormField() also expects members 'max_length' and
// 'is_checked' to exist.
- (void)fillField:(const std::string&)fieldIdentifier
         formName:(const std::string&)formName
            value:(const base::string16)value
          inFrame:(web::WebFrame*)frame {
  auto data = std::make_unique<base::DictionaryValue>();
  data->SetString("identifier", fieldIdentifier);
  data->SetString("form", formName);
  data->SetString("value", value);

  DCHECK(suggestionHandledCompletion_);
  [jsAutofillManager_ fillActiveFormField:std::move(data)
                                  inFrame:frame
                        completionHandler:suggestionHandledCompletion_];
  suggestionHandledCompletion_ = nil;
}

// Sends the the |data| to |frame| to actually fill the data.
- (void)sendData:(std::unique_ptr<base::Value>)data
         toFrame:(web::WebFrame*)frame {
  DCHECK(webState_->IsVisible());
  // It is possible that the fill was not initiated by selecting a suggestion.
  // In this case we provide an empty callback.
  if (!suggestionHandledCompletion_)
    suggestionHandledCompletion_ = [^{
    } copy];
  [jsAutofillManager_ fillForm:std::move(data)
      forceFillFieldIdentifier:base::SysUTF16ToNSString(
                                   pendingAutocompleteField_)
                       inFrame:frame
             completionHandler:suggestionHandledCompletion_];
  suggestionHandledCompletion_ = nil;
}

@end
