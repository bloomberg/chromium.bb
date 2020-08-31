// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/prefilled_values_detector.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

using blink::ToWebInputElement;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using blink::WebVector;
using blink::WebView;

namespace autofill {

using form_util::FindFormControlElementByUniqueRendererId;
using form_util::FindFormControlElementsByUniqueRendererId;
using form_util::IsFormControlVisible;
using form_util::IsFormVisible;

using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

// The size above which we stop triggering autocomplete.
const size_t kMaximumTextSizeForAutocomplete = 1000;

// Names of HTML attributes to show form and field signatures for debugging.
const char kDebugAttributeForFormSignature[] = "form_signature";
const char kDebugAttributeForFieldSignature[] = "field_signature";
const char kDebugAttributeForParserAnnotations[] = "pm_parser_annotation";

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

typedef std::vector<FormInputElementMap> FormElementsList;

bool IsElementEditable(const WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool DoUsernamesMatch(const base::string16& potential_suggestion,
                      const base::string16& current_username,
                      bool exact_match) {
  if (potential_suggestion == current_username)
    return true;
  return !exact_match && IsPrefixOfEmailEndingWithAtSign(current_username,
                                                         potential_suggestion);
}

// Returns whether the |username_element| is allowed to be autofilled.
//
// Note that if the user interacts with the |password_field| and the
// |username_element| is user-defined (i.e., non-empty and non-autofilled), then
// this function returns false. This is a precaution, to not override the field
// if it has been classified as username by accident.
bool IsUsernameAmendable(const WebInputElement& username_element,
                         bool is_password_field_selected) {
  return !username_element.IsNull() && IsElementEditable(username_element) &&
         (!is_password_field_selected || username_element.IsAutofilled() ||
          username_element.Value().IsEmpty());
}

// Log `message` if `logger` is not null.
void LogMessage(Logger* logger, Logger::StringID message) {
  if (logger)
    logger->LogMessage(message);
}

// Log `message` and `value` if `logger` is not null.
void LogBoolean(Logger* logger, Logger::StringID message, bool value) {
  if (logger)
    logger->LogBoolean(message, value);
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(Logger* logger,
                 Logger::StringID message_id,
                 const WebFormElement& form) {
  if (logger) {
    logger->LogHTMLForm(message_id, form.GetName().Utf8(),
                        GURL(form.Action().Utf8()));
  }
}

// Returns true if there are any suggestions to be derived from |fill_data|.
// Only considers suggestions with usernames having |typed_username| as prefix.
bool CanShowUsernameSuggestion(const PasswordFormFillData& fill_data,
                               const base::string16& typed_username) {
  base::string16 typed_username_lower = base::i18n::ToLower(typed_username);
  if (base::StartsWith(base::i18n::ToLower(fill_data.username_field.value),
                       typed_username_lower, base::CompareCase::SENSITIVE)) {
    return true;
  }

  for (const auto& login : fill_data.additional_logins) {
    if (base::StartsWith(base::i18n::ToLower(login.username),
                         typed_username_lower, base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

// This function attempts to find the matching credentials for the
// |current_username| by scanning |fill_data|. The result is written in
// |username| and |password| parameters.
void FindMatchesByUsername(const PasswordFormFillData& fill_data,
                           const base::string16& current_username,
                           bool exact_username_match,
                           RendererSavePasswordProgressLogger* logger,
                           base::string16* username,
                           base::string16* password) {
  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.username_field.value, current_username,
                       exact_username_match)) {
    *username = fill_data.username_field.value;
    *password = fill_data.password_field.value;
    LogMessage(logger, Logger::STRING_USERNAMES_MATCH);
  } else {
    // Scan additional logins for a match.
    for (const auto& it : fill_data.additional_logins) {
      if (!it.realm.empty()) {
        // Non-empty realm means PSL match. Do not autofill PSL matched
        // credentials. The reason for this is that PSL matched sites are
        // different sites, so a password for a PSL matched site should be never
        // filled without explicit user selection.
        continue;
      }
      if (DoUsernamesMatch(it.username, current_username,
                           exact_username_match)) {
        *username = it.username;
        *password = it.password;
        break;
      }
    }
    LogBoolean(logger, Logger::STRING_MATCH_IN_ADDITIONAL,
               !(username->empty() && password->empty()));
  }
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

// Helper function that calculates form signature for |password_form| and
// returns it as WebString.
WebString GetFormSignatureAsWebString(const FormData& form_data) {
  return WebString::FromUTF8(
      base::NumberToString(CalculateFormSignature(form_data).value()));
}

// Annotate |fields| with field signatures and form signature as HTML
// attributes.
void AnnotateFieldsWithSignatures(
    std::vector<blink::WebFormControlElement>* fields,
    const blink::WebString& form_signature) {
  for (blink::WebFormControlElement& control_element : *fields) {
    FieldSignature field_signature = CalculateFieldSignatureByNameAndType(
        control_element.NameForAutofill().Utf16(),
        control_element.FormControlTypeForAutofill().Utf8());
    control_element.SetAttribute(
        WebString::FromASCII(kDebugAttributeForFieldSignature),
        WebString::FromUTF8(base::NumberToString(field_signature.value())));
    control_element.SetAttribute(
        blink::WebString::FromASCII(kDebugAttributeForFormSignature),
        form_signature);
  }
}

// Annotate |forms| and all fields in the |frame| with form and field signatures
// as HTML attributes.
void AnnotateFormsAndFieldsWithSignatures(WebLocalFrame* frame,
                                          WebVector<WebFormElement>* forms) {
  for (WebFormElement& form : *forms) {
    std::unique_ptr<FormData> form_data(
        CreateFormDataFromWebForm(form, /*field_data_manager=*/nullptr,
                                  /*username_detector_cache=*/nullptr));
    WebString form_signature;
    if (form_data) {
      form_signature = GetFormSignatureAsWebString(*form_data);
      form.SetAttribute(WebString::FromASCII(kDebugAttributeForFormSignature),
                        form_signature);
    }
    std::vector<WebFormControlElement> form_fields =
        form_util::ExtractAutofillableElementsInForm(form);
    AnnotateFieldsWithSignatures(&form_fields, form_signature);
  }

  std::vector<WebFormControlElement> unowned_elements =
      form_util::GetUnownedAutofillableFormFieldElements(
          frame->GetDocument().All(), nullptr);
  std::unique_ptr<FormData> form_data(CreateFormDataFromUnownedInputElements(
      *frame, /*field_data_manager=*/nullptr,
      /*username_detector_cache=*/nullptr));
  WebString form_signature;
  if (form_data)
    form_signature = GetFormSignatureAsWebString(*form_data);
  AnnotateFieldsWithSignatures(&unowned_elements, form_signature);
}

// Returns true iff there is a password field in |frame|.
bool HasPasswordField(const WebLocalFrame& frame) {
  static base::NoDestructor<WebString> kPassword("password");

  const WebElementCollection elements = frame.GetDocument().All();
  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.IsFormControlElement()) {
      const WebFormControlElement& control =
          element.To<WebFormControlElement>();
      if (control.FormControlTypeForAutofill() == *kPassword)
        return true;
    }
  }
  return false;
}

// Returns the closest visible autocompletable non-password text element
// preceding the |password_element| either in a form, if it belongs to one, or
// in the |frame|.
WebInputElement FindUsernameElementPrecedingPasswordElement(
    WebLocalFrame* frame,
    const WebInputElement& password_element) {
  DCHECK(!password_element.IsNull());

  std::vector<WebFormControlElement> elements;
  if (password_element.Form().IsNull()) {
    elements = form_util::GetUnownedAutofillableFormFieldElements(
        frame->GetDocument().All(), nullptr);
  } else {
    elements = password_element.Form().GetFormControlElements().ReleaseVector();
  }

  auto iter = std::find(elements.begin(), elements.end(), password_element);
  if (iter == elements.end())
    return WebInputElement();

  for (auto begin = elements.begin(); iter != begin;) {
    --iter;
    const WebInputElement* input = ToWebInputElement(&*iter);
    if (input && input->IsTextField() && !input->IsPasswordFieldForAutofill() &&
        IsElementEditable(*input) && form_util::IsWebElementVisible(*input)) {
      return *input;
    }
  }

  return WebInputElement();
}

WebInputElement ConvertToWebInput(const WebFormControlElement& element) {
  if (element.IsNull())
    return WebInputElement();
  const WebInputElement* input = ToWebInputElement(&element);
  return input ? *input : WebInputElement();
}

// Returns true if |element|'s frame origin is not PSL matched with the origin
// of any parent frame.
bool IsInCrossOriginIframe(const WebInputElement& element) {
  WebFrame* cur_frame = element.GetDocument().GetFrame();
  WebString bottom_frame_origin = cur_frame->GetSecurityOrigin().ToString();

  DCHECK(cur_frame);

  while (cur_frame->Parent()) {
    cur_frame = cur_frame->Parent();
    if (!IsPublicSuffixDomainMatch(
            bottom_frame_origin.Utf8(),
            cur_frame->GetSecurityOrigin().ToString().Utf8())) {
      return true;
    }
  }
  return false;
}

// Whether any of the fields in |form| is a password field.
bool FormHasPasswordField(const FormData& form) {
  for (const auto& field : form.fields) {
    if (field.IsPasswordInputElement())
      return true;
  }
  return false;
}

// Whether any of the fields in |form| is a non-empty password field.
bool FormHasNonEmptyPasswordField(const FormData& form) {
  for (const auto& field : form.fields) {
    if (field.IsPasswordInputElement()) {
      if (!field.value.empty() || !field.typed_value.empty())
        return true;
    }
  }
  return false;
}

void AnnotateFieldWithParsingResult(WebDocument doc,
                                    FieldRendererId renderer_id,
                                    const std::string& text) {
  if (renderer_id.is_null())
    return;
  auto element = FindFormControlElementByUniqueRendererId(doc, renderer_id);
  if (element.IsNull())
    return;
  element.SetAttribute(
      WebString::FromASCII(kDebugAttributeForParserAnnotations),
      WebString::FromASCII(text));
}

bool HasDocumentWithValidFrame(const WebInputElement& element) {
  WebFrame* frame = element.GetDocument().GetFrame();
  return frame && frame->View();
}

bool ShowPopupWithoutPasswords(const WebInputElement& password_element) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return false;
  }
  return !password_element.IsNull() && IsElementEditable(password_element);
}

// This method tries to fix `fields` with empty typed or filled properties by
// matching them against previously filled or typed in fields with the same
// value and copying their filled or typed mask.
//
// This helps against websites where submitted fields differ from fields that
// had previously been autofilled or typed into.
void FillNonTypedOrFilledPropertiesMasks(std::vector<FormFieldData>* fields,
                                         const FieldDataManager& manager) {
  static constexpr FieldPropertiesMask kFilledOrTyped =
      FieldPropertiesFlags::kAutofilled | FieldPropertiesFlags::kUserTyped;

  for (auto& field : *fields) {
    if (field.properties_mask & kFilledOrTyped)
      continue;

    for (const auto& pair : manager.field_data_map()) {
      const auto& field_data = pair.second;

      if ((field_data.second & kFilledOrTyped) &&
          field_data.first == field.value) {
        field.properties_mask |= field_data.second & kFilledOrTyped;
        break;
      }
    }
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      last_supplied_password_info_iter_(web_input_to_password_info_.end()),
      field_data_manager_(base::MakeRefCounted<FieldDataManager>()),
      logging_state_active_(false),
      username_autofill_state_(WebAutofillState::kNotFilled),
      password_autofill_state_(WebAutofillState::kNotFilled),
      sent_request_to_store_(false),
      checked_safe_browsing_reputation_(false),
      focus_state_notifier_(this),
      password_generation_agent_(nullptr) {
  registry->AddInterface(base::BindRepeating(
      &PasswordAutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
  AutofillAgent* agent = autofill_agent_.get();
  if (agent)
    agent->RemoveFormObserver(this);
}

void PasswordAutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::PasswordAutofillAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PasswordAutofillAgent::SetAutofillAgent(AutofillAgent* autofill_agent) {
  AutofillAgent* agent = autofill_agent_.get();
  if (agent)
    agent->RemoveFormObserver(this);
  autofill_agent_ = autofill_agent->GetWeakPtr();
  autofill_agent->AddFormObserver(this);
}

void PasswordAutofillAgent::SetPasswordGenerationAgent(
    PasswordGenerationAgent* generation_agent) {
  password_generation_agent_ = generation_agent;
}

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo() = default;

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo(
    const FormStructureInfo& other) = default;

PasswordAutofillAgent::FormStructureInfo&
PasswordAutofillAgent::FormStructureInfo::operator=(
    const PasswordAutofillAgent::FormStructureInfo& other) = default;

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo(
    FormStructureInfo&& other) = default;

PasswordAutofillAgent::FormStructureInfo&
PasswordAutofillAgent::FormStructureInfo::operator=(
    PasswordAutofillAgent::FormStructureInfo&& other) = default;

PasswordAutofillAgent::FormStructureInfo::~FormStructureInfo() = default;

PasswordAutofillAgent::FocusStateNotifier::FocusStateNotifier(
    PasswordAutofillAgent* agent)
    : agent_(agent) {}

PasswordAutofillAgent::FocusStateNotifier::~FocusStateNotifier() = default;

void PasswordAutofillAgent::FocusStateNotifier::FocusedInputChanged(
    FocusedFieldType focused_field_type) {
  // Forward the request if the type changed or the field is fillable.
  if (focused_field_type != focused_field_type_ ||
      IsFillable(focused_field_type)) {
    agent_->GetPasswordManagerDriver()->FocusedInputChanged(focused_field_type);
  }

  focused_field_type_ = focused_field_type;
}

PasswordAutofillAgent::PasswordValueGatekeeper::PasswordValueGatekeeper()
    : was_user_gesture_seen_(false) {}

PasswordAutofillAgent::PasswordValueGatekeeper::~PasswordValueGatekeeper() =
    default;

void PasswordAutofillAgent::PasswordValueGatekeeper::RegisterElement(
    WebInputElement* element) {
  if (was_user_gesture_seen_)
    ShowValue(element);
  else
    elements_.push_back(*element);
}

void PasswordAutofillAgent::PasswordValueGatekeeper::OnUserGesture() {
  if (was_user_gesture_seen_)
    return;

  was_user_gesture_seen_ = true;

  for (WebInputElement& element : elements_)
    ShowValue(&element);

  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    WebInputElement* element) {
  if (!element->IsNull() && !element->SuggestedValue().IsEmpty()) {
    element->SetAutofillValue(element->SuggestedValue());
    element->SetAutofillState(WebAutofillState::kAutofilled);
  }
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const WebInputElement& element) {
  // TODO(crbug.com/415449): Do this through const WebInputElement.
  WebInputElement mutable_element = element;  // We need a non-const.
  mutable_element.SetAutofillState(WebAutofillState::kNotFilled);

  auto iter = web_input_to_password_info_.find(element);
  if (iter != web_input_to_password_info_.end()) {
    iter->second.password_was_edited_last = false;
  }

  // Show the popup with the list of available usernames.
  return ShowSuggestions(element, ShowAll(false), GenerationShowing(false));
}

void PasswordAutofillAgent::DidEndTextFieldEditing() {
  focus_state_notifier_.FocusedInputChanged(FocusedFieldType::kUnknown);
}

void PasswordAutofillAgent::UpdateStateForTextChange(
    const WebInputElement& element) {
  if (!element.IsTextField())
    return;
  // TODO(crbug.com/415449): Do this through const WebInputElement.
  WebInputElement mutable_element = element;  // We need a non-const.

  const base::string16 element_value = element.Value().Utf16();
  field_data_manager_->UpdateFieldDataMap(element, element_value,
                                          FieldPropertiesFlags::kUserTyped);

  ProvisionallySavePassword(element.Form(), element, RESTRICTION_NONE);

  if (element.IsPasswordFieldForAutofill()) {
    auto iter = password_to_username_.find(element);
    if (iter != password_to_username_.end()) {
      web_input_to_password_info_[iter->second].password_was_edited_last = true;
      // Note that the suggested value of |mutable_element| was reset when its
      // value changed.
      mutable_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
    GetPasswordManagerDriver()->UserModifiedPasswordField();
  } else {
    GetPasswordManagerDriver()->UserModifiedNonPasswordField(
        FieldRendererId(element.UniqueRendererFormControlId()), element_value);
  }
}

bool PasswordAutofillAgent::FillSuggestion(
    const WebFormControlElement& control_element,
    const base::string16& username,
    const base::string16& password) {
  // The element in context of the suggestion popup.
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;

  if (!FindPasswordInfoForElement(*element, UseFallbackData(true),
                                  &username_element, &password_element,
                                  &password_info) ||
      (!password_element.IsNull() && !IsElementEditable(password_element))) {
    return false;
  }

  password_info->password_was_edited_last = false;
  if (element->IsPasswordFieldForAutofill()) {
    password_info->password_field_suggestion_was_accepted = true;
    password_info->password_field = password_element;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (!password_element.IsNull() && password_generation_agent_)
    password_generation_agent_->OnFieldAutofilled(password_element);

  if (IsUsernameAmendable(username_element,
                          element->IsPasswordFieldForAutofill()) &&
      !(username.empty() && element->IsPasswordFieldForAutofill()) &&
      username_element.Value().Utf16() != username) {
    FillField(&username_element, username);
  }

  if (!password_element.IsNull())
    FillPasswordFieldAndSave(&password_element, password);

  WebInputElement mutable_filled_element = *element;
  mutable_filled_element.SetSelectionRange(element->Value().length(),
                                           element->Value().length());

  return true;
}

void PasswordAutofillAgent::FillIntoFocusedField(
    bool is_password,
    const base::string16& credential) {
  if (focused_input_element_.IsNull())
    return;
  if (!is_password) {
    FillField(&focused_input_element_, credential);
  }
  if (!focused_input_element_.IsPasswordFieldForAutofill())
    return;
  FillPasswordFieldAndSave(&focused_input_element_, credential);
}

void PasswordAutofillAgent::FillField(WebInputElement* input,
                                      const base::string16& credential) {
  DCHECK(input);
  DCHECK(!input->IsNull());
  input->SetAutofillValue(WebString::FromUTF16(credential));
  input->SetAutofillState(WebAutofillState::kAutofilled);
  field_data_manager_->UpdateFieldDataMap(
      *input, credential, FieldPropertiesFlags::kAutofilledOnUserTrigger);
}

void PasswordAutofillAgent::FillPasswordFieldAndSave(
    WebInputElement* password_input,
    const base::string16& credential) {
  DCHECK(password_input);
  DCHECK(password_input->IsPasswordFieldForAutofill());
  FillField(password_input, credential);
  ProvisionallySavePassword(password_input->Form(), *password_input,
                            RESTRICTION_NONE);
}

bool PasswordAutofillAgent::PreviewSuggestion(
    const WebFormControlElement& control_element,
    const WebString& username,
    const WebString& password) {
  // The element in context of the suggestion popup.
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, UseFallbackData(true),
                                  &username_element, &password_element,
                                  &password_info) ||
      (!password_element.IsNull() && !IsElementEditable(password_element))) {
    return false;
  }

  if (IsUsernameAmendable(username_element,
                          element->IsPasswordFieldForAutofill())) {
    if (username_query_prefix_.empty())
      username_query_prefix_ = username_element.Value().Utf16();

    username_autofill_state_ = username_element.GetAutofillState();
    username_element.SetSuggestedValue(username);
    username_element.SetAutofillState(WebAutofillState::kPreviewed);
    form_util::PreviewSuggestion(username_element.SuggestedValue().Utf16(),
                                 username_query_prefix_, &username_element);
  }
  if (!password_element.IsNull()) {
    password_autofill_state_ = password_element.GetAutofillState();
    password_element.SetSuggestedValue(password);
    password_element.SetAutofillState(WebAutofillState::kPreviewed);
  }

  return true;
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const WebFormControlElement& control_element) {
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, UseFallbackData(true),
                                  &username_element, &password_element,
                                  &password_info)) {
    return false;
  }

  ClearPreview(&username_element, &password_element);
  return true;
}

bool PasswordAutofillAgent::FindPasswordInfoForElement(
    const WebInputElement& element,
    UseFallbackData use_fallback_data,
    WebInputElement* username_element,
    WebInputElement* password_element,
    PasswordInfo** password_info) {
  DCHECK(username_element && password_element && password_info);
  username_element->Reset();
  password_element->Reset();
  if (!element.IsPasswordFieldForAutofill()) {
    *username_element = element;
  } else {
    *password_element = element;

    // If there is a password field, but a request to the store hasn't been sent
    // yet, then do fetch saved credentials now.
    if (!sent_request_to_store_) {
      SendPasswordForms(false);
      return false;
    }

    auto iter = web_input_to_password_info_.find(element);
    if (iter == web_input_to_password_info_.end()) {
      PasswordToLoginMap::const_iterator password_iter =
          password_to_username_.find(element);
      if (password_iter == password_to_username_.end()) {
        if (!use_fallback_data || web_input_to_password_info_.empty())
          return false;
        iter = last_supplied_password_info_iter_;
      } else {
        *username_element = password_iter->second;
      }
    }

    if (iter != web_input_to_password_info_.end()) {
      // It's a password field without corresponding username field. Try to find
      // the username field based on visibility.
      *username_element = FindUsernameElementPrecedingPasswordElement(
          render_frame()->GetWebFrame(), *password_element);
      *password_info = &iter->second;
      return true;
    }
    // Otherwise |username_element| has been set above.
  }

  auto iter = web_input_to_password_info_.find(*username_element);
  if (iter == web_input_to_password_info_.end())
    return false;

  *password_info = &iter->second;
  if (password_element->IsNull())
    *password_element = (*password_info)->password_field;

  return true;
}

void PasswordAutofillAgent::MaybeCheckSafeBrowsingReputation(
    const WebInputElement& element) {
  // Enabled on desktop and Android
#if BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  // Note: A site may use a Password field to collect a CVV or a Credit Card
  // number, but showing a slightly misleading warning here is better than
  // showing no warning at all.
  if (!element.IsPasswordFieldForAutofill())
    return;
  if (checked_safe_browsing_reputation_)
    return;

  checked_safe_browsing_reputation_ = true;
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  GURL frame_url = GURL(frame->GetDocument().Url());
  GURL action_url = element.Form().IsNull()
                        ? GURL()
                        : form_util::GetCanonicalActionForForm(element.Form());
  GetPasswordManagerDriver()->CheckSafeBrowsingReputation(action_url,
                                                          frame_url);
#endif
}

bool PasswordAutofillAgent::ShouldSuppressKeyboard() {
  // The keyboard should be suppressed if we are showing the Touch To Fill UI.
  return touch_to_fill_state_ == TouchToFillState::kIsShowing;
}

bool PasswordAutofillAgent::TryToShowTouchToFill(
    const WebFormControlElement& control_element) {
  // Don't show Touch To Fill if it should only be enabled for insecure origins
  // and we are currently on a potentially trustworthy origin.
  if (base::GetFieldTrialParamByFeatureAsBool(features::kAutofillTouchToFill,
                                              "insecure-origins-only",
                                              /*default_value=*/false) &&
      render_frame()
          ->GetWebFrame()
          ->GetDocument()
          .GetSecurityOrigin()
          .IsPotentiallyTrustworthy()) {
    return false;
  }

  if (touch_to_fill_state_ != TouchToFillState::kShouldShow)
    return false;

  const WebInputElement* input_element = ToWebInputElement(&control_element);
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  if (!input_element ||
      !FindPasswordInfoForElement(*input_element, UseFallbackData(false),
                                  &username_element, &password_element,
                                  &password_info)) {
    return false;
  }

  // Don't trigger Touch To Fill when there is no password element or it is not
  // editable.
  if (password_element.IsNull() || !IsElementEditable(password_element))
    return false;

  // Highlight the fields that are about to be filled by the user and remember
  // the old autofill state of |username_element| and |password_element|.
  if (IsUsernameAmendable(username_element,
                          input_element->IsPasswordFieldForAutofill())) {
    username_autofill_state_ = username_element.GetAutofillState();
    username_element.SetAutofillState(WebAutofillState::kPreviewed);
  }

  password_autofill_state_ = password_element.GetAutofillState();
  password_element.SetAutofillState(WebAutofillState::kPreviewed);

  focused_input_element_ = *input_element;
  GetPasswordManagerDriver()->ShowTouchToFill();
  touch_to_fill_state_ = TouchToFillState::kIsShowing;
  return true;
}

bool PasswordAutofillAgent::ShowSuggestions(
    const WebInputElement& element,
    ShowAll show_all,
    GenerationShowing generation_popup_showing) {
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  FindPasswordInfoForElement(element, UseFallbackData(true), &username_element,
                             &password_element, &password_info);

  if (!password_info && !ShowPopupWithoutPasswords(password_element)) {
    MaybeCheckSafeBrowsingReputation(element);
    return false;
  }

  // Check that all fillable elements are editable.
  if (!element.IsTextField() || !IsElementEditable(element) ||
      (!password_element.IsNull() && !IsElementEditable(password_element))) {
    return true;
  }

  // Don't attempt to autofill with values that are too large.
  if (element.Value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  if (generation_popup_showing)
    return false;

  // Don't call ShowSuggestionPopup if Touch To Fill is currently showing. Since
  // Touch To Fill in spirit is very similar to a suggestion pop-up, return true
  // so that the AutofillAgent does not try to show other autofill suggestions
  // instead.
  if (touch_to_fill_state_ == TouchToFillState::kIsShowing)
    return true;

  if (!HasDocumentWithValidFrame(element))
    return false;

  // If a username element is focused, show suggestions unless all possible
  // usernames are filtered.
  if (!element.IsPasswordFieldForAutofill()) {
    if (show_all ||
        (password_info && CanShowUsernameSuggestion(password_info->fill_data,
                                                    element.Value().Utf16()))) {
      ShowSuggestionPopup(element.Value().Utf16(), element, show_all,
                          OnPasswordField(false));
      return true;
    }
    return false;
  }

  // If the element is a password field, do not to show a popup if the user has
  // already accepted a password suggestion on another password field.
  if (password_info && password_info->password_field_suggestion_was_accepted &&
      element != password_info->password_field) {
    return true;
  }

  // Show suggestions for password fields only while they are empty.
  if (!element.IsAutofilled() && !element.Value().IsEmpty()) {
    HidePopup();
    return false;
  }

  ShowSuggestionPopup(base::string16(), element, show_all,
                      OnPasswordField(true));
  return true;
}

bool PasswordAutofillAgent::FrameCanAccessPasswordManager() {
  // about:blank or about:srcdoc frames should not be allowed to use password
  // manager.  See https://crbug.com/756587.
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  blink::WebURL url = frame->GetDocument().Url();
  if (!url.ProtocolIs(url::kHttpScheme) && !url.ProtocolIs(url::kHttpsScheme))
    return false;
  return frame->GetSecurityOrigin().CanAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen() {
  SendPasswordForms(false /* only_visible */);
}

void PasswordAutofillAgent::FireSubmissionIfFormDisappear(
    SubmissionIndicatorEvent event) {
  if (!browser_has_form_to_process_)
    return;
  DCHECK(FrameCanAccessPasswordManager());

  // Prompt to save only if the form is now gone, either invisible or
  // removed from the DOM.
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // TODO(crbug.com/720347): This method could be called often and checking form
  // visibility could be expensive. Add performance metrics for this.
  if (event != SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR) {
    bool is_last_updated_field_in_form =
        !last_updated_form_renderer_id_.is_null();
    // Check whether the form which is the candidate for submission disappeared.
    // If yes this form is considered to be successfully submitted.
    if (is_last_updated_field_in_form) {
      // A form is inside <form> tag. Check the visibility of the whole form.
      if (IsFormVisible(frame, last_updated_form_renderer_id_))
        return;
    } else {
      // A form is without <form> tag. Check the visibility of the last updated
      // field.
      if (IsFormControlVisible(frame, last_updated_field_renderer_id_))
        return;
    }
  }
  GetPasswordManagerDriver()->SameDocumentNavigation(event);
  browser_has_form_to_process_ = false;
}

void PasswordAutofillAgent::UserGestureObserved() {
  autofilled_elements_cache_.clear();

  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::SendPasswordForms(bool only_visible) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.ToString().Utf8()));
  }
  if (!FrameCanAccessPasswordManager()) {
    LogMessage(logger.get(), Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (form_util::IsWebpageEmpty(frame)) {
    LogMessage(logger.get(), Logger::STRING_WEBPAGE_EMPTY);
    return;
  }

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();

  if (IsShowAutofillSignaturesEnabled())
    AnnotateFormsAndFieldsWithSignatures(frame, &forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<FormData> password_forms_data;
  for (const WebFormElement& form : forms) {
    if (only_visible) {
      bool is_form_visible = form_util::AreFormContentsVisible(form);
      LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
      LogBoolean(logger.get(), Logger::STRING_FORM_IS_VISIBLE, is_form_visible);

      // If requested, ignore non-rendered forms, e.g., those styled with
      // display:none.
      if (!is_form_visible)
        continue;
    }

    std::unique_ptr<FormData> form_data(GetFormDataFromWebForm(form));
    if (!form_data || !FormHasPasswordField(*form_data))
      continue;

    if (logger)
      logger->LogFormData(Logger::STRING_FORM_IS_PASSWORD, *form_data);

    FormStructureInfo form_structure_info =
        ExtractFormStructureInfo(*form_data);
    if (only_visible || WasFormStructureChanged(form_structure_info)) {
      forms_structure_cache_[form_structure_info.unique_renderer_id] =
          std::move(form_structure_info);

      password_forms_data.push_back(std::move(*form_data));
      continue;
    }

    std::vector<WebFormControlElement> control_elements =
        form.GetFormControlElements().ReleaseVector();
    // Sometimes JS can change autofilled forms. In this case we try to restore
    // values for the changed elements.
    TryFixAutofilledForm(&control_elements);
  }

  // See if there are any unattached input elements that could be used for
  // password submission.
  // TODO(crbug/898109): Consider using TryFixAutofilledForm for the cases when
  // there is no form tag.
  bool add_unowned_inputs = true;
  if (only_visible) {
    std::vector<WebFormControlElement> control_elements =
        form_util::GetUnownedAutofillableFormFieldElements(
            frame->GetDocument().All(), nullptr);
    add_unowned_inputs =
        form_util::IsSomeControlElementVisible(control_elements);
    LogBoolean(logger.get(), Logger::STRING_UNOWNED_INPUTS_VISIBLE,
               add_unowned_inputs);
  }

  if (add_unowned_inputs) {
    std::unique_ptr<FormData> form_data(GetFormDataFromUnownedInputElements());
    if (form_data) {
      if (logger) {
        logger->LogFormData(Logger::STRING_FORM_IS_PASSWORD, *form_data);
      }
      password_forms_data.push_back(std::move(*form_data));
    }
  }

  if (only_visible) {
    // Send the PasswordFormsRendered message regardless of whether
    // |password_forms_data| is empty. The empty |password_forms_data| are a
    // possible signal to the browser that a pending login attempt succeeded.
    WebFrame* main_frame = render_frame()->GetWebFrame()->Top();
    bool did_stop_loading = !main_frame || !main_frame->IsLoading();
    GetPasswordManagerDriver()->PasswordFormsRendered(password_forms_data,
                                                      did_stop_loading);
  } else {
    // If there is a password field, but the list of password forms is empty for
    // some reason, add a dummy form to the list. It will cause a request to the
    // store. Therefore, saved passwords will be available for filling on click.
    if (!sent_request_to_store_ && password_forms_data.empty() &&
        HasPasswordField(*frame)) {
      // Set everything that |FormDigest| needs.
      password_forms_data.push_back(FormData());
      password_forms_data.back().url =
          form_util::GetCanonicalOriginForDocument(frame->GetDocument());
      password_forms_data.back().full_url =
          form_util::GetOriginWithoutAuthForDocument(frame->GetDocument());
    }
    if (!password_forms_data.empty()) {
      sent_request_to_store_ = true;
      GetPasswordManagerDriver()->PasswordFormsParsed(password_forms_data);
    }
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Provide warnings about the accessibility of password forms on the page.
  if (!password_forms_data.empty() &&
      (frame->GetDocument().Url().ProtocolIs(url::kHttpScheme) ||
       frame->GetDocument().Url().ProtocolIs(url::kHttpsScheme)))
    page_passwords_analyser_.AnalyseDocumentDOM(frame);
#endif
}

void PasswordAutofillAgent::DidFinishDocumentLoad() {
  SendPasswordForms(false);
}

void PasswordAutofillAgent::DidFinishLoad() {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(true);
}

void PasswordAutofillAgent::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (!is_same_document_navigation) {
    checked_safe_browsing_reputation_ = false;
    recorded_first_filling_result_ = false;
  }
}

void PasswordAutofillAgent::OnFrameDetached() {
  // If a sub frame has been destroyed while the user was entering information
  // into a password form, try to save the data. See https://crbug.com/450806
  // for examples of sites that perform login using this technique.
  if (render_frame()->GetWebFrame()->Parent() && browser_has_form_to_process_) {
    DCHECK(FrameCanAccessPasswordManager());
    GetPasswordManagerDriver()->SameDocumentNavigation(
        SubmissionIndicatorEvent::FRAME_DETACHED);
  }
  CleanupOnDocumentShutdown();
}

void PasswordAutofillAgent::OnDestruct() {
  receiver_.reset();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PasswordAutofillAgent::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  if (navigated_frame->Parent()) {
    LogMessage(logger.get(), Logger::STRING_FRAME_NOT_MAIN_FRAME);
  } else {
    // This is a new navigation, so require a new user gesture before filling in
    // passwords.
    gatekeeper_.Reset();
  }

  CleanupOnDocumentShutdown();
}

void PasswordAutofillAgent::OnProbablyFormSubmitted() {}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::FillPasswordForm(
    const PasswordFormFillData& form_data) {
  DCHECK(form_data.has_renderer_ids);
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_ON_FILL_PASSWORD_FORM_METHOD);
  }

  bool username_password_fields_not_set =
      form_data.username_field.unique_renderer_id.is_null() &&
      form_data.password_field.unique_renderer_id.is_null();
  if (username_password_fields_not_set) {
    // No fields for filling were found during parsing, which means filling
    // fallback case. So save data for fallback filling.
    MaybeStoreFallbackData(form_data);
    return;
  }

  WebInputElement username_element, password_element;
  std::tie(username_element, password_element) =
      FindUsernamePasswordElements(form_data);
  bool is_single_username_fill =
      form_data.password_field.unique_renderer_id.is_null();
  WebElement main_element =
      is_single_username_fill ? username_element : password_element;
  if (main_element.IsNull()) {
    MaybeStoreFallbackData(form_data);
    // TODO(https://crbug.com/959776): Fix logging for single username.
    LogFirstFillingResult(form_data, FillingResult::kNoPasswordElement);
    return;
  }

  StoreDataForFillOnAccountSelect(form_data, username_element,
                                  password_element);

  // If wait_for_username is true, we don't want to initially fill the form
  // until the user types in a valid username.
  if (form_data.wait_for_username) {
    LogFirstFillingResult(form_data, FillingResult::kWaitForUsername);
    return;
  }

  FillUserNameAndPassword(username_element, password_element, form_data,
                          logger.get());
}

void PasswordAutofillAgent::SetLoggingState(bool active) {
  logging_state_active_ = active;
}

void PasswordAutofillAgent::TouchToFillClosed(bool show_virtual_keyboard) {
  touch_to_fill_state_ = TouchToFillState::kWasShown;

  // Clear the autofill state from the username and password element. Note that
  // we don't make use of ClearPreview() here, since this is considering the
  // elements' SuggestedValue(), which Touch To Fill does not set.
  DCHECK(!focused_input_element_.IsNull());
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  if (!FindPasswordInfoForElement(focused_input_element_, UseFallbackData(true),
                                  &username_element, &password_element,
                                  &password_info)) {
    return;
  }

  if (!username_element.IsNull())
    username_element.SetAutofillState(username_autofill_state_);

  if (!password_element.IsNull())
    password_element.SetAutofillState(password_autofill_state_);

  if (show_virtual_keyboard) {
    render_frame()->ShowVirtualKeyboard();

    // Since Touch To Fill suppresses the Autofill popup, re-trigger the
    // suggestions in case the virtual keyboard should be shown. This is limited
    // to the keyboard accessory, as otherwise it would result in a flickering
    // of the popup, due to showing the keyboard at the same time.
    if (IsKeyboardAccessoryEnabled()) {
      ShowSuggestions(focused_input_element_, ShowAll(false),
                      GenerationShowing(false));
    }
  }
}

void PasswordAutofillAgent::AnnotateFieldsWithParsingResult(
    const ParsingResult& parsing_result) {
  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  AnnotateFieldWithParsingResult(doc, parsing_result.username_renderer_id,
                                 "username_element");
  AnnotateFieldWithParsingResult(doc, parsing_result.password_renderer_id,
                                 "password_element");
  AnnotateFieldWithParsingResult(doc, parsing_result.new_password_renderer_id,
                                 "new_password_element");
  AnnotateFieldWithParsingResult(doc,
                                 parsing_result.confirm_password_renderer_id,
                                 "confirmation_password_element");
}

void PasswordAutofillAgent::InformNoSavedCredentials() {
  autofilled_elements_cache_.clear();

  // Clear the actual field values.
  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  std::vector<WebFormControlElement> elements =
      FindFormControlElementsByUniqueRendererId(
          doc, std::vector<FieldRendererId>(all_autofilled_elements_.begin(),
                                            all_autofilled_elements_.end()));
  for (WebFormControlElement& element : elements) {
    if (element.IsNull())
      continue;
    element.SetSuggestedValue(blink::WebString());
    // Don't clear the actual value of fields that the user has edited manually
    // (which changes the autofill state back to kNotFilled).
    if (element.GetAutofillState() == WebAutofillState::kAutofilled)
      element.SetValue(blink::WebString());
    element.SetAutofillState(WebAutofillState::kNotFilled);
  }
  all_autofilled_elements_.clear();

  field_data_manager_->ClearData();
}

void PasswordAutofillAgent::FocusedNodeHasChanged(const blink::WebNode& node) {
  DCHECK(!node.IsNull());
  focused_input_element_.Reset();
  if (!node.IsElementNode()) {
    focus_state_notifier_.FocusedInputChanged(FocusedFieldType::kUnknown);
    return;
  }

  auto element = node.ToConst<WebElement>();
  if (element.IsFormControlElement() &&
      form_util::IsTextAreaElement(element.ToConst<WebFormControlElement>())) {
    focus_state_notifier_.FocusedInputChanged(
        FocusedFieldType::kFillableTextArea);
    return;
  }

  auto* input_element = ToWebInputElement(&element);
  if (!input_element) {
    focus_state_notifier_.FocusedInputChanged(
        FocusedFieldType::kUnfillableElement);
    return;
  }

  auto focused_field_type = FocusedFieldType::kUnfillableElement;
  if (input_element->IsTextField() && IsElementEditable(*input_element)) {
    focused_input_element_ = *input_element;

    WebString type = input_element->GetAttribute("type");
    if (!type.IsNull() && type == "search")
      focused_field_type = FocusedFieldType::kFillableSearchField;
    else if (input_element->IsPasswordFieldForAutofill())
      focused_field_type = FocusedFieldType::kFillablePasswordField;
    else if (base::Contains(web_input_to_password_info_, *input_element))
      focused_field_type = FocusedFieldType::kFillableUsernameField;
    else
      focused_field_type = FocusedFieldType::kFillableNonSearchField;
  }

  focus_state_notifier_.FocusedInputChanged(focused_field_type);
  field_data_manager_->UpdateFieldDataMapWithNullValue(
      *input_element, FieldPropertiesFlags::kHadFocus);
}

std::unique_ptr<FormData> PasswordAutofillAgent::GetFormDataFromWebForm(
    const WebFormElement& web_form) {
  return CreateFormDataFromWebForm(web_form, field_data_manager_.get(),
                                   &username_detector_cache_);
}

std::unique_ptr<FormData>
PasswordAutofillAgent::GetFormDataFromUnownedInputElements() {
  // The element's frame might have been detached in the meantime (see
  // http://crbug.com/585363, comments 5 and 6), in which case |frame| will
  // be null. This was hardly caused by form submission (unless the user is
  // supernaturally quick), so it is OK to drop the ball here.
  content::RenderFrame* frame = render_frame();
  if (!frame)
    return nullptr;
  WebLocalFrame* web_frame = frame->GetWebFrame();
  if (!web_frame)
    return nullptr;
  return CreateFormDataFromUnownedInputElements(
      *web_frame, field_data_manager_.get(), &username_detector_cache_);
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

void PasswordAutofillAgent::ShowSuggestionPopup(
    const base::string16& typed_username,
    const WebInputElement& user_input,
    ShowAll show_all,
    OnPasswordField show_on_password_field) {
  username_query_prefix_ = typed_username;
  FormData form;
  FormFieldData field;
  form_util::FindFormAndFieldForFormControlElement(
      user_input, field_data_manager_.get(), &form, &field);

  int options = 0;
  if (show_all)
    options |= SHOW_ALL;
  if (show_on_password_field)
    options |= IS_PASSWORD_FIELD;

  GetPasswordManagerDriver()->ShowPasswordSuggestions(
      field.text_direction, typed_username, options,
      render_frame()->ElementBoundsInWindow(user_input));
}

void PasswordAutofillAgent::CleanupOnDocumentShutdown() {
  web_input_to_password_info_.clear();
  password_to_username_.clear();
  last_supplied_password_info_iter_ = web_input_to_password_info_.end();
  browser_has_form_to_process_ = false;
  field_data_manager_.get()->ClearData();
  username_autofill_state_ = WebAutofillState::kNotFilled;
  password_autofill_state_ = WebAutofillState::kNotFilled;
  sent_request_to_store_ = false;
  checked_safe_browsing_reputation_ = false;
  username_query_prefix_.clear();
  username_detector_cache_.clear();
  forms_structure_cache_.clear();
  autofilled_elements_cache_.clear();
  all_autofilled_elements_.clear();
  last_updated_field_renderer_id_ = FieldRendererId();
  last_updated_form_renderer_id_ = FormRendererId();
  touch_to_fill_state_ = TouchToFillState::kShouldShow;
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  page_passwords_analyser_.Reset();
#endif
}

void PasswordAutofillAgent::ClearPreview(WebInputElement* username,
                                         WebInputElement* password) {
  if (!username->IsNull() && !username->SuggestedValue().IsEmpty()) {
    username->SetSuggestedValue(WebString());
    username->SetAutofillState(username_autofill_state_);
    username->SetSelectionRange(username_query_prefix_.length(),
                                username->Value().length());
  }
  if (!password->IsNull() && !password->SuggestedValue().IsEmpty()) {
    password->SetSuggestedValue(WebString());
    password->SetAutofillState(password_autofill_state_);
  }
}
void PasswordAutofillAgent::ProvisionallySavePassword(
    const WebFormElement& form,
    const WebInputElement& element,
    ProvisionallySaveRestriction restriction) {
  DCHECK(!form.IsNull() || !element.IsNull());
  SetLastUpdatedFormAndField(form, element);
  std::unique_ptr<FormData> form_data =
      (form.IsNull() ? GetFormDataFromUnownedInputElements()
                     : GetFormDataFromWebForm(form));
  if (!form_data)
    return;

  bool has_password = FormHasNonEmptyPasswordField(*form_data);
  if (restriction == RESTRICTION_NON_EMPTY_PASSWORD && !has_password)
    return;

  if (!FrameCanAccessPasswordManager())
    return;

  if (has_password)
    GetPasswordManagerDriver()->ShowManualFallbackForSaving(*form_data);
  else
    GetPasswordManagerDriver()->HideManualFallbackForSaving();

  browser_has_form_to_process_ = true;
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    WebInputElement username_element,
    WebInputElement password_element,
    const PasswordFormFillData& fill_data,
    RendererSavePasswordProgressLogger* logger) {
  LogMessage(logger, Logger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD);

  bool is_single_username_fill = password_element.IsNull();
  WebInputElement main_element =
      is_single_username_fill ? username_element : password_element;

  if (IsInCrossOriginIframe(main_element)) {
    LogMessage(logger, Logger::STRING_FAILED_TO_FILL_INTO_IFRAME);
    LogFirstFillingResult(fill_data, FillingResult::kBlockedByFrameHierarchy);
    return false;
  }

  // Don't fill username if password can't be set.
  if (!IsElementEditable(main_element)) {
    LogMessage(logger,
               Logger::STRING_FAILED_TO_FILL_NO_AUTOCOMPLETEABLE_ELEMENT);
    LogFirstFillingResult(fill_data,
                          FillingResult::kPasswordElementIsNotAutocompleteable);
    return false;
  }

  // |current_username| is the username for credentials that are going to be
  // autofilled. It is selected according to the algorithm:
  // 1. If the page already contains a non-empty value in |username_element|
  // that is not found in the list of values known to be used as placeholders,
  // this is adopted and not overridden.
  // 2. Default username from |fill_data| if the username field is
  // autocompletable.
  // 3. Empty if username field doesn't exist or if username field is empty and
  // not autocompletable (no username case).
  base::string16 current_username;

  // Whether the username element was prefilled with content that was on a
  // list of known placeholder texts that should be overridden (e.g. "username
  // or email" or there is a server hint that it is just a placeholder).
  bool prefilled_placeholder_username = false;

  if (!username_element.IsNull()) {
    // This is a heuristic guess. If the credential is stored for
    // www.example.com, the username may be prefilled with "@example.com".
    std::string possible_email_domain =
        GetRegistryControlledDomain(fill_data.origin);

    prefilled_placeholder_username =
        !username_element.Value().IsEmpty() &&
        (PossiblePrefilledUsernameValue(username_element.Value().Utf8(),
                                        possible_email_domain) ||
         fill_data.username_may_use_prefilled_placeholder);
    if (!username_element.Value().IsEmpty() &&
        username_element.GetAutofillState() == WebAutofillState::kNotFilled &&
        !prefilled_placeholder_username) {
      // Username is filled with content that was not on a list of known
      // placeholder texts (e.g. "username or email") nor there is server-side
      // data that this value is placeholder.
      current_username = username_element.Value().Utf16();
    } else if (IsElementEditable(username_element)) {
      current_username = fill_data.username_field.value;
    }
  }

  // |username| and |password| will contain the match found if any.
  base::string16 username;
  base::string16 password;

  bool exact_username_match =
      username_element.IsNull() || IsElementEditable(username_element);

  FindMatchesByUsername(fill_data, current_username, exact_username_match,
                        logger, &username, &password);

  if (password.empty() && !is_single_username_fill) {
    if (!username_element.IsNull() && !username_element.Value().IsEmpty() &&
        !prefilled_placeholder_username) {
      LogPrefilledUsernameFillOutcome(
          PrefilledUsernameFillOutcome::kPrefilledUsernameNotOverridden);

      LogMessage(logger, Logger::STRING_FAILED_TO_FILL_PREFILLED_USERNAME);
      LogFirstFillingResult(
          fill_data, FillingResult::kUsernamePrefilledWithIncompatibleValue);
      return false;
    }
    LogMessage(logger,
               Logger::STRING_FAILED_TO_FILL_FOUND_NO_PASSWORD_FOR_USERNAME);
    LogFirstFillingResult(fill_data,
                          FillingResult::kFoundNoPasswordForUsername);
    return false;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (password_generation_agent_ && !is_single_username_fill)
    password_generation_agent_->OnFieldAutofilled(password_element);

  // Input matches the username, fill in required values.
  if (!username_element.IsNull() && IsElementEditable(username_element)) {
    if (!username.empty() &&
        (username_element.Value().IsEmpty() ||
         username_element.GetAutofillState() != WebAutofillState::kNotFilled ||
         prefilled_placeholder_username)) {
      AutofillField(username, username_element);
      if (prefilled_placeholder_username) {
        LogPrefilledUsernameFillOutcome(
            PrefilledUsernameFillOutcome::
                kPrefilledPlaceholderUsernameOverridden);
      }
    }
    if (logger)
      logger->LogElementName(Logger::STRING_USERNAME_FILLED, username_element);
  }

  if (!is_single_username_fill) {
    AutofillField(password, password_element);
    if (logger)
      logger->LogElementName(Logger::STRING_PASSWORD_FILLED, password_element);
  }

  LogFirstFillingResult(fill_data, FillingResult::kSuccess);
  return true;
}

void PasswordAutofillAgent::LogPrefilledUsernameFillOutcome(
    PrefilledUsernameFillOutcome outcome) {
  if (prefilled_username_metrics_logged_)
    return;
  prefilled_username_metrics_logged_ = true;
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PrefilledUsernameFillOutcome",
                            outcome);
}

void PasswordAutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    ElementChangeSource source) {
  // PasswordAutofillAgent isn't interested in SELECT control change.
  if (source == ElementChangeSource::SELECT_CHANGED)
    return;

  WebInputElement input_element;
  if (!element.IsNull() && element.HasHTMLTagName("input"))
    input_element = *ToWebInputElement(&element);

  if (source == ElementChangeSource::TEXTFIELD_CHANGED) {
    DCHECK(!input_element.IsNull());
    // keeps track of all text changes even if it isn't displaying UI.
    UpdateStateForTextChange(input_element);
    return;
  }

  DCHECK_EQ(ElementChangeSource::WILL_SEND_SUBMIT_EVENT, source);
  ProvisionallySavePassword(form, input_element,
                            RESTRICTION_NON_EMPTY_PASSWORD);
}

void PasswordAutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  if (!FrameCanAccessPasswordManager()) {
    LogMessage(logger.get(), Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  std::unique_ptr<FormData> submitted_form_data = GetFormDataFromWebForm(form);

  if (!submitted_form_data)
    return;

  submitted_form_data->submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  FillNonTypedOrFilledPropertiesMasks(&submitted_form_data->fields,
                                      *field_data_manager_);

  GetPasswordManagerDriver()->PasswordFormSubmitted(*submitted_form_data);
  browser_has_form_to_process_ = false;
}

void PasswordAutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  if (source == SubmissionSource::FRAME_DETACHED) {
    OnFrameDetached();
  } else {
    SubmissionIndicatorEvent event = ToSubmissionIndicatorEvent(source);
    if (event == SubmissionIndicatorEvent::NONE)
      return;
    FireSubmissionIfFormDisappear(event);
  }
}

void PasswordAutofillAgent::HidePopup() {
  if (autofill_agent_)
    autofill_agent_->GetAutofillDriver()->HidePopup();
}

const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
PasswordAutofillAgent::GetPasswordManagerDriver() {
  if (!password_manager_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_manager_driver_);
  }
  return password_manager_driver_;
}

std::pair<WebInputElement, WebInputElement>
PasswordAutofillAgent::FindUsernamePasswordElements(
    const PasswordFormFillData& form_data) {
  const FieldRendererId username_renderer_id =
      form_data.username_field.unique_renderer_id;
  const FieldRendererId password_renderer_id =
      form_data.password_field.unique_renderer_id;
  const bool is_username_present = !username_renderer_id.is_null();
  const bool is_password_present = !password_renderer_id.is_null();

  std::vector<FieldRendererId> element_ids;
  if (is_password_present)
    element_ids.push_back(password_renderer_id);
  if (is_username_present)
    element_ids.push_back(username_renderer_id);

  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  bool wrapped_in_form_tag = !form_data.form_renderer_id.is_null();
  std::vector<WebFormControlElement> elements =
      wrapped_in_form_tag
          ? form_util::FindFormControlElementsByUniqueRendererId(
                doc, form_data.form_renderer_id, element_ids)
          : form_util::FindFormControlElementsByUniqueRendererId(doc,
                                                                 element_ids);

  // Set password element.
  WebInputElement password_field;
  size_t current_index = 0;
  if (is_password_present)
    password_field = ConvertToWebInput(elements[current_index++]);

  // Set username element.
  WebInputElement username_field;
  if (is_username_present)
    username_field = ConvertToWebInput(elements[current_index++]);

  return std::make_pair(username_field, password_field);
}

void PasswordAutofillAgent::StoreDataForFillOnAccountSelect(
    const PasswordFormFillData& form_data,
    WebInputElement username_element,
    WebInputElement password_element) {
  WebInputElement main_element =
      username_element.IsNull() ? password_element : username_element;

  PasswordInfo password_info;
  password_info.fill_data = form_data;
  password_info.password_field = password_element;
  web_input_to_password_info_[main_element] = password_info;
  last_supplied_password_info_iter_ =
      web_input_to_password_info_.find(main_element);
  if (!main_element.IsPasswordFieldForAutofill())
    password_to_username_[password_element] = username_element;
}

void PasswordAutofillAgent::MaybeStoreFallbackData(
    const PasswordFormFillData& form_data) {
  if (!web_input_to_password_info_.empty())
    return;
  // If for some reasons elements for filling were not found (for example
  // because they were renamed by JavaScript) then add fill data for
  // |web_input_to_password_info_|. When the user clicks on a password field
  // which is not a key in |web_input_to_password_info_|, the first element from
  // |web_input_to_password_info_| will be used in
  // PasswordAutofillAgent::FindPasswordInfoForElement to propose to fill.
  PasswordInfo password_info;
  password_info.fill_data = form_data;
  web_input_to_password_info_[WebInputElement()] = password_info;
  last_supplied_password_info_iter_ = web_input_to_password_info_.begin();
}

void PasswordAutofillAgent::LogFirstFillingResult(
    const PasswordFormFillData& form_data,
    FillingResult result) {
  if (recorded_first_filling_result_)
    return;
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FirstRendererFillingResult",
                            result);
  DCHECK(form_data.has_renderer_ids);
  GetPasswordManagerDriver()->LogFirstFillingResult(
      form_data.form_renderer_id, base::strict_cast<int32_t>(result));
  recorded_first_filling_result_ = true;
}

PasswordAutofillAgent::FormStructureInfo
PasswordAutofillAgent::ExtractFormStructureInfo(const FormData& form_data) {
  FormStructureInfo result;
  result.unique_renderer_id = form_data.unique_renderer_id;
  result.fields.resize(form_data.fields.size());

  for (size_t i = 0; i < form_data.fields.size(); ++i) {
    const FormFieldData& form_field = form_data.fields[i];

    FormFieldInfo& field_info = result.fields[i];
    field_info.unique_renderer_id = form_field.unique_renderer_id;
    field_info.form_control_type = form_field.form_control_type;
    field_info.autocomplete_attribute = form_field.autocomplete_attribute;
    field_info.is_focusable = form_field.is_focusable;
  }

  return result;
}

bool PasswordAutofillAgent::WasFormStructureChanged(
    const FormStructureInfo& form_info) const {
  if (form_info.unique_renderer_id.is_null())
    return true;

  auto cached_form = forms_structure_cache_.find(form_info.unique_renderer_id);
  if (cached_form == forms_structure_cache_.end())
    return true;

  const FormStructureInfo& cached_form_info = cached_form->second;

  if (form_info.fields.size() != cached_form_info.fields.size())
    return true;

  for (size_t i = 0; i < form_info.fields.size(); ++i) {
    const FormFieldInfo& form_field = form_info.fields[i];
    const FormFieldInfo& cached_form_field = cached_form_info.fields[i];

    if (form_field.unique_renderer_id != cached_form_field.unique_renderer_id)
      return true;

    if (form_field.form_control_type != cached_form_field.form_control_type)
      return true;

    if (form_field.autocomplete_attribute !=
        cached_form_field.autocomplete_attribute) {
      return true;
    }

    if (form_field.is_focusable != cached_form_field.is_focusable)
      return true;
  }
  return false;
}

void PasswordAutofillAgent::TryFixAutofilledForm(
    std::vector<WebFormControlElement>* control_elements) const {
  for (auto& element : *control_elements) {
    const FieldRendererId element_id(element.UniqueRendererFormControlId());
    auto cached_element = autofilled_elements_cache_.find(element_id);
    if (cached_element == autofilled_elements_cache_.end())
      continue;

    const WebString& cached_value = cached_element->second;
    if (cached_value != element.SuggestedValue())
      element.SetSuggestedValue(cached_value);
  }
}

void PasswordAutofillAgent::AutofillField(const base::string16& value,
                                          WebInputElement field) {
  // Do not autofill on load fields that have any user typed input.
  const FieldRendererId field_id(field.UniqueRendererFormControlId());
  if (field_data_manager_->DidUserType(field_id))
    return;

  if (field.Value().Utf16() == value)
    return;

  field.SetSuggestedValue(WebString::FromUTF16(value));
  field.SetAutofillState(WebAutofillState::kAutofilled);
  // Wait to fill until a user gesture occurs. This is to make sure that we do
  // not fill in the DOM with a password until we believe the user is
  // intentionally interacting with the page.
  gatekeeper_.RegisterElement(&field);
  field_data_manager_.get()->UpdateFieldDataMap(
      field, value, FieldPropertiesFlags::kAutofilledOnPageLoad);
  autofilled_elements_cache_.emplace(field_id, WebString::FromUTF16(value));
  all_autofilled_elements_.insert(field_id);
}

void PasswordAutofillAgent::SetLastUpdatedFormAndField(
    const WebFormElement& form,
    const WebFormControlElement& input) {
  last_updated_form_renderer_id_ =
      form.IsNull() ? FormRendererId()
                    : FormRendererId(form.UniqueRendererFormId());
  last_updated_field_renderer_id_ =
      input.IsNull() ? FieldRendererId()
                     : FieldRendererId(input.UniqueRendererFormControlId());
}

}  // namespace autofill
