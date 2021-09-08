// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_assistant_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/cert/cert_status_flags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebAutofillState;
using blink::WebAXObject;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

using form_util::ExtractMask;
using form_util::FindFormAndFieldForFormControlElement;
using form_util::UnownedFormElementsAndFieldSetsToFormData;
using mojom::SubmissionSource;
using ShowAll = PasswordAutofillAgent::ShowAll;
using GenerationShowing = PasswordAutofillAgent::GenerationShowing;

namespace {

// Time to wait, in ms, o ensure that only a single select change will be acted
// upon, instead of multiple in close succession (debounce time).
size_t kWaitTimeForSelectOptionsChangesMs = 50;

// Helper function to return EXTRACT_DATALIST if kAutofillExtractAllDatalist is
// enabled, otherwise EXTRACT_NONE is returned.
ExtractMask GetExtractDatalistMask() {
  return base::FeatureList::IsEnabled(features::kAutofillExtractAllDatalists)
             ? form_util::EXTRACT_DATALIST
             : form_util::EXTRACT_NONE;
}

}  // namespace

AutofillAgent::ShowSuggestionsOptions::ShowSuggestionsOptions()
    : autofill_on_empty_values(false),
      requires_caret_at_end(false),
      show_full_suggestion_list(false),
      autoselect_first_suggestion(false) {}

AutofillAgent::AutofillAgent(content::RenderFrame* render_frame,
                             PasswordAutofillAgent* password_autofill_agent,
                             PasswordGenerationAgent* password_generation_agent,
                             AutofillAssistantAgent* autofill_assistant_agent,
                             blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      form_cache_(render_frame->GetWebFrame()),
      password_autofill_agent_(password_autofill_agent),
      password_generation_agent_(password_generation_agent),
      autofill_assistant_agent_(autofill_assistant_agent),
      autofill_query_id_(0),
      query_node_autofill_state_(WebAutofillState::kNotFilled),
      is_popup_possibly_visible_(false),
      is_generation_popup_possibly_visible_(false),
      is_user_gesture_required_(true),
      is_secure_context_required_(false),
      form_tracker_(render_frame),
      field_data_manager_(password_autofill_agent->GetFieldDataManager()) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent->SetAutofillAgent(this);
  AddFormObserver(this);
  registry->AddInterface(base::BindRepeating(
      &AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAgent::~AutofillAgent() {
  RemoveFormObserver(this);
}

void AutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAgent::DidCommitProvisionalLoad(ui::PageTransition transition) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // TODO(dvadym): check if we need to check if it is main frame navigation
  // http://crbug.com/443155
  if (frame->Parent())
    return;  // Not a top-level navigation.

  // Navigation to a new page or a page refresh.

  element_.Reset();

  form_cache_.Reset();
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidFinishDocumentLoad() {
  ProcessForms();
}

void AutofillAgent::DidChangeScrollOffset() {
  if (element_.IsNull())
    return;

  if (!focus_requires_scroll_) {
    // Post a task here since scroll offset may change during layout.
    // (https://crbug.com/804886)
    weak_ptr_factory_.InvalidateWeakPtrs();
    render_frame()
        ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&AutofillAgent::DidChangeScrollOffsetImpl,
                                  weak_ptr_factory_.GetWeakPtr(), element_));
  } else {
    HidePopup();
  }
}

void AutofillAgent::DidChangeScrollOffsetImpl(
    const WebFormControlElement& element) {
  if (element != element_ || element_.IsNull() || focus_requires_scroll_ ||
      !is_popup_possibly_visible_ || !element_.Focused())
    return;

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element_, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver()->TextFieldDidScroll(form, field, field.bounds);
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

void AutofillAgent::FocusedElementChanged(const WebElement& element) {
  was_focused_before_now_ = false;
  HidePopup();

  if (element.IsNull()) {
    // Focus moved away from the last interacted form (if any) to somewhere else
    // on the page.
    GetAutofillDriver()->FocusNoLongerOnForm(!last_interacted_form_.IsNull());
    return;
  }

  const WebInputElement* input = ToWebInputElement(&element);

  bool focus_moved_to_new_form = false;
  if (!last_interacted_form_.IsNull() &&
      (!input || last_interacted_form_ != input->Form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    GetAutofillDriver()->FocusNoLongerOnForm(/*had_interacted_form=*/true);
    focus_moved_to_new_form = true;
  }

  // Calls HandleFocusChangeComplete() after notifying the focus is no longer on
  // the previous form, then early return. No need to notify the newly focused
  // element because that will be done by HandleFocusChangeComplete() which
  // triggers FormControlElementClicked().
  // Refer to http://crbug.com/1105254
  if ((IsKeyboardAccessoryEnabled() || !focus_requires_scroll_) &&
      !element.IsNull() &&
      element.GetDocument().GetFrame()->HasTransientUserActivation()) {
    focused_node_was_last_clicked_ = true;
    HandleFocusChangeComplete();
  }

  if (focus_moved_to_new_form)
    return;

  if (!input || !input->IsEnabled() || input->IsReadOnly() ||
      !input->IsTextField())
    return;

  element_ = *input;

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element_, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver()->FocusOnFormField(form, field, field.bounds);
  }
}

void AutofillAgent::OnDestruct() {
  Shutdown();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AutofillAgent::AccessibilityModeChanged(const ui::AXMode& mode) {
  is_screen_reader_enabled_ = mode.has_mode(ui::AXMode::kScreenReader);
}

void AutofillAgent::FireHostSubmitEvents(const WebFormElement& form,
                                         bool known_success,
                                         SubmissionSource source) {
  FormData form_data;
  if (!form_util::ExtractFormData(form, *field_data_manager_.get(), &form_data))
    return;

  FireHostSubmitEvents(form_data, known_success, source);
}

void AutofillAgent::FireHostSubmitEvents(const FormData& form_data,
                                         bool known_success,
                                         SubmissionSource source) {
  // We don't want to fire duplicate submission event.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAllowDuplicateFormSubmissions) &&
      !submitted_forms_.insert(form_data.unique_renderer_id).second) {
    return;
  }

  GetAutofillDriver()->FormSubmitted(form_data, known_success, source);
}

void AutofillAgent::Shutdown() {
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AutofillAgent::TextFieldDidEndEditing(const WebInputElement& element) {
  // Sometimes "blur" events are side effects of the password generation
  // handling the page. They should not affect any UI in the browser.
  if (password_generation_agent_ &&
      password_generation_agent_->ShouldIgnoreBlur()) {
    return;
  }
  GetAutofillDriver()->DidEndTextFieldEditing();
  password_autofill_agent_->DidEndTextFieldEditing();
  if (password_generation_agent_)
    password_generation_agent_->DidEndTextFieldEditing(element);

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::SetUserGestureRequired(bool required) {
  form_tracker_.set_user_gesture_required(required);
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_.TextFieldDidChange(element);
}

void AutofillAgent::OnTextFieldDidChange(const WebInputElement& element) {
  if (password_generation_agent_ &&
      password_generation_agent_->TextDidChangeInTextField(element)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (password_autofill_agent_->TextDidChangeInTextField(element)) {
    is_popup_possibly_visible_ = true;
    element_ = element;
    return;
  }

  ShowSuggestionsOptions options;
  options.requires_caret_at_end = true;
  ShowSuggestions(element, options);

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver()->TextFieldDidChange(form, field, field.bounds,
                                            AutofillTickClock::NowTicks());
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestionsOptions options;
    options.autofill_on_empty_values = true;
    options.requires_caret_at_end = true;
    options.autoselect_first_suggestion =
        ShouldAutoselectFirstSuggestionOnArrowDown();
    ShowSuggestions(element, options);
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  ShowSuggestions(element, options);
}

void AutofillAgent::DataListOptionsChanged(const WebInputElement& element) {
  if (!is_popup_possibly_visible_ || !element.Focused())
    return;

  OnProvisionallySaveForm(WebFormElement(), element,
                          ElementChangeSource::TEXTFIELD_CHANGED);
}

void AutofillAgent::UserGestureObserved() {
  password_autofill_agent_->UserGestureObserved();
}

void AutofillAgent::TriggerRefillIfNeeded(const FormData& form) {
  ReplaceElementIfNowInvalid(form);

  FormFieldData field;
  FormData updated_form;
  if (FindFormAndFieldForFormControlElement(element_, field_data_manager_.get(),
                                            &updated_form, &field) &&
      (!element_.IsAutofilled() || !form.DynamicallySameFormAs(updated_form))) {
    WebLocalFrame* frame = render_frame()->GetWebFrame();
    std::vector<FormData> forms;
    forms.push_back(updated_form);
    // Always communicate to browser process for topmost frame.
    if (!forms.empty() || !frame->Parent()) {
      GetAutofillDriver()->FormsSeen(forms);
    }
  }
}

// mojom::AutofillAgent:
void AutofillAgent::FillForm(int32_t id, const FormData& form) {
  if (element_.IsNull())
    return;

  if (id != autofill_query_id_ && id != kNoQueryId)
    return;

  was_last_action_fill_ = true;

  // If this is a re-fill, replace the triggering element if it's invalid.
  if (id == kNoQueryId)
    ReplaceElementIfNowInvalid(form);

  query_node_autofill_state_ = element_.GetAutofillState();
  form_util::FillForm(form, element_);
  if (!element_.Form().IsNull())
    UpdateLastInteractedForm(element_.Form());

  // TODO(crbug.com/1198811): Inform the BrowserAutofillManager about the fields
  // that were actually filled. It's possible that the form has changed since
  // the time filling was triggered.
  GetAutofillDriver()->DidFillAutofillFormData(form,
                                               AutofillTickClock::NowTicks());

  TriggerRefillIfNeeded(form);
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::PreviewForm(int32_t id, const FormData& form) {
  if (element_.IsNull())
    return;

  if (id != autofill_query_id_)
    return;

  ClearPreviewedForm();

  query_node_autofill_state_ = element_.GetAutofillState();
  previewed_elements_ = form_util::PreviewForm(form, element_);

  GetAutofillDriver()->DidPreviewAutofillFormData();
}

void AutofillAgent::FieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  bool attach_predictions_to_dom =
      base::FeatureList::IsEnabled(features::kAutofillShowTypePredictions);
  for (const auto& form : forms) {
    form_cache_.ShowPredictions(form, attach_predictions_to_dom);
  }
}

void AutofillAgent::ClearSection() {
  if (element_.IsNull())
    return;

  form_cache_.ClearSectionWithElement(element_);
}

void AutofillAgent::ClearPreviewedForm() {
  // TODO(crbug.com/816533): It is very rare, but it looks like the |element_|
  // can be null if a provisional load was committed immediately prior to
  // clearing the previewed form.
  if (element_.IsNull())
    return;

  if (password_autofill_agent_->DidClearAutofillSelection(element_))
    return;

  form_util::ClearPreviewedElements(previewed_elements_, element_,
                                    query_node_autofill_state_);
  previewed_elements_ = {};
}

void AutofillAgent::FillFieldWithValue(FieldRendererId field_id,
                                       const std::u16string& value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element) {
    DoFillFieldWithValue(value, input_element);
    input_element->SetAutofillState(WebAutofillState::kAutofilled);
  }
}

void AutofillAgent::PreviewFieldWithValue(FieldRendererId field_id,
                                          const std::u16string& value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element)
    DoPreviewFieldWithValue(value, input_element);
}

void AutofillAgent::SetSuggestionAvailability(
    FieldRendererId field_id,
    const mojom::AutofillState state) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element) {
    switch (state) {
      case autofill::mojom::AutofillState::kAutofillAvailable:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutofillAvailable);
        return;
      case autofill::mojom::AutofillState::kAutocompleteAvailable:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutocompleteAvailable);
        return;
      case autofill::mojom::AutofillState::kNoSuggestions:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kNoSuggestions);
        return;
    }
    NOTREACHED();
  }
}

void AutofillAgent::AcceptDataListSuggestion(
    FieldRendererId field_id,
    const std::u16string& suggested_value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (!input_element) {
    // For reasons not understood yet, this is triggered on elements which are
    // not input elements.

    // TODO(crbug.com/1048270) Gather debug data.
    DEBUG_ALIAS_FOR_CSTR(element_name, element_.TagName().Latin1().c_str(), 64);
    base::debug::DumpWithoutCrashing();

    // Keep this return after removing the TODO(crbug.com/1048270) above.
    return;
  }
  std::u16string new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (input_element->IsMultiple() && input_element->IsEmailField()) {
    std::u16string value = input_element->EditingValue().Utf16();
    std::vector<base::StringPiece16> parts = base::SplitStringPiece(
        value, u",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() == 0)
      parts.push_back(base::StringPiece16());

    std::u16string last_part(parts.back());
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!base::IsUnicodeWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts.back() = last_part;

    new_value = base::JoinString(parts, u",");
  }
  DoFillFieldWithValue(new_value, input_element);
}

void AutofillAgent::FillPasswordSuggestion(const std::u16string& username,
                                           const std::u16string& password) {
  if (element_.IsNull())
    return;

  bool handled =
      password_autofill_agent_->FillSuggestion(element_, username, password);
  DCHECK(handled);
}

void AutofillAgent::PreviewPasswordSuggestion(const std::u16string& username,
                                              const std::u16string& password) {
  if (element_.IsNull())
    return;

  bool handled = password_autofill_agent_->PreviewSuggestion(
      element_, blink::WebString::FromUTF16(username),
      blink::WebString::FromUTF16(password));
  DCHECK(handled);
}

bool AutofillAgent::CollectFormlessElements(FormData* output) const {
  if (render_frame() == nullptr || render_frame()->GetWebFrame() == nullptr)
    return false;

  WebDocument document = render_frame()->GetWebFrame()->GetDocument();

  // Build up the FormData from the unowned elements. This logic mostly
  // mirrors the construction of the synthetic form in form_cache.cc, but
  // happens at submit-time so we can capture the modifications the user
  // has made, and doesn't depend on form_cache's internal state.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document.All(),
                                                         &fieldsets);

  if (control_elements.size() > kMaxParseableFields)
    return false;

  const ExtractMask extract_mask = static_cast<ExtractMask>(
      form_util::EXTRACT_VALUE | form_util::EXTRACT_OPTIONS);

  return UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, nullptr, document, field_data_manager_.get(),
      extract_mask, output, nullptr);
}

void AutofillAgent::ShowSuggestions(const WebFormControlElement& element,
                                    const ShowSuggestionsOptions& options) {
  if (!element.IsEnabled() || element.IsReadOnly())
    return;
  if (!element.SuggestedValue().IsEmpty())
    return;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (input_element) {
    if (!input_element->IsTextField())
      return;
    if (!input_element->SuggestedValue().IsEmpty())
      return;
  } else {
    DCHECK(form_util::IsTextAreaElement(element));
    if (!element.ToConst<WebFormControlElement>().SuggestedValue().IsEmpty())
      return;
  }

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.EditingValue();
  if (value.length() > kMaxDataLength ||
      (!options.autofill_on_empty_values && value.IsEmpty()) ||
      (options.requires_caret_at_end &&
       (element.SelectionStart() != element.SelectionEnd() ||
        element.SelectionEnd() != static_cast<int>(value.length())))) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  element_ = element;
  if (form_util::IsAutofillableInputElement(input_element) &&
      password_autofill_agent_->ShowSuggestions(
          *input_element, ShowAll(options.show_full_suggestion_list),
          GenerationShowing(is_generation_popup_possibly_visible_))) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (is_generation_popup_possibly_visible_)
    return;

  // Password field elements should only have suggestions shown by the password
  // autofill agent.
  // The /*disable presubmit*/ comment below is used to disable a presubmit
  // script that ensures that only IsPasswordFieldForAutofill() is used in this
  // code (it has to appear between the function name and the parentesis to not
  // match a regex). In this specific case we are actually interested in whether
  // the field is currently a password field, not whether it has ever been a
  // password field.
  if (input_element &&
      input_element->IsPasswordField /*disable presubmit*/ () &&
      !query_password_suggestion_) {
    return;
  }

  QueryAutofillSuggestions(element, options.autoselect_first_suggestion);
}

void AutofillAgent::SetQueryPasswordSuggestion(bool query) {
  query_password_suggestion_ = query;
}

void AutofillAgent::SetSecureContextRequired(bool required) {
  is_secure_context_required_ = required;
}

void AutofillAgent::SetFocusRequiresScroll(bool require) {
  focus_requires_scroll_ = require;
}

void AutofillAgent::GetElementFormAndFieldDataAtIndex(
    const std::string& selector,
    int index,
    GetElementFormAndFieldDataAtIndexCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  blink::WebElement target_element;
  blink::WebVector<blink::WebElement> elements =
      render_frame()->GetWebFrame()->GetDocument().QuerySelectorAll(
          blink::WebString::FromUTF8(selector));
  if (index >= 0 && static_cast<size_t>(index) < elements.size()) {
    target_element = elements[index];
  }

  FormData form;
  FormFieldData field;
  if (base::FeatureList::IsEnabled(features::kAutofillAugmentFormsInRenderer)) {
    form.host_frame = LocalFrameToken(frame->GetLocalFrameToken().value());
    field.host_frame = LocalFrameToken(frame->GetLocalFrameToken().value());
  }

  if (target_element.IsNull() || !target_element.IsFormControlElement()) {
    return std::move(callback).Run(form, field);
  }

  blink::WebFormControlElement target_form_control_element =
      target_element.To<blink::WebFormControlElement>();
  bool success = FindFormAndFieldForFormControlElement(
      target_form_control_element, field_data_manager_.get(), &form, &field);
  if (success) {
    // Remember this element so as to autofill the form without focusing the
    // field for Autofill Assistant.
    element_ = target_form_control_element;
  }
  // Do not expect failure.
  DCHECK(success);

  return std::move(callback).Run(form, field);
}

void AutofillAgent::SetAssistantActionState(bool running) {
  DCHECK(autofill_assistant_agent_);
  if (running) {
    autofill_assistant_agent_->DisableKeyboard();
  } else {
    autofill_assistant_agent_->EnableKeyboard();
  }
}

void AutofillAgent::EnableHeavyFormDataScraping() {
  is_heavy_form_data_scraping_enabled_ = true;
}

void AutofillAgent::SetFieldsEligibleForManualFilling(
    const std::vector<FieldRendererId>& fields) {
  form_cache_.SetFieldsEligibleForManualFilling(fields);
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    bool autoselect_first_suggestion) {
  blink::WebLocalFrame* frame = element.GetDocument().GetFrame();
  if (!frame)
    return;

  DCHECK(ToWebInputElement(&element) || form_util::IsTextAreaElement(element));

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForFormControlElement(
          element, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillAugmentFormsInRenderer)) {
      // |form| may be only partially initialized and may be sent to the browser
      // in this state. Set at least the |host_frame| because sending an empty
      // base::UnguessableToken is illegal.
      form.host_frame = LocalFrameToken(frame->GetLocalFrameToken().value());
    }
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(
        element, nullptr,
        static_cast<ExtractMask>(form_util::EXTRACT_VALUE |
                                 form_util::EXTRACT_BOUNDS |
                                 GetExtractDatalistMask()),
        &field);
  }

  if (is_secure_context_required_ &&
      !(element.GetDocument().IsSecureContext())) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kAutofillExtractAllDatalists)) {
    if (const WebInputElement* input_element = ToWebInputElement(&element)) {
      // Find the datalist values and send them to the browser process.
      form_util::GetDataListSuggestions(*input_element, &field.datalist_values,
                                        &field.datalist_labels);
    }
  }

  is_popup_possibly_visible_ = true;
  GetAutofillDriver()->QueryFormFieldAutofill(autofill_query_id_, form, field,
                                              field.bounds,
                                              autoselect_first_suggestion);
}

void AutofillAgent::DoFillFieldWithValue(const std::u16string& value,
                                         WebInputElement* node) {
  form_tracker_.set_ignore_control_changes(true);
  node->SetAutofillValue(blink::WebString::FromUTF16(value));
  password_autofill_agent_->UpdateStateForTextChange(*node);
  form_tracker_.set_ignore_control_changes(false);
}

void AutofillAgent::DoPreviewFieldWithValue(const std::u16string& value,
                                            WebInputElement* node) {
  ClearPreviewedForm();
  query_node_autofill_state_ = element_.GetAutofillState();
  node->SetSuggestedValue(blink::WebString::FromUTF16(value));
  node->SetAutofillState(WebAutofillState::kPreviewed);
  form_util::PreviewSuggestion(node->SuggestedValue().Utf16(),
                               node->Value().Utf16(), node);
  previewed_elements_.push_back(*node);
}

void AutofillAgent::ProcessForms() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  std::vector<FormData> forms =
      form_cache_.ExtractNewForms(field_data_manager_.get());

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->Parent()) {
    GetAutofillDriver()->FormsSeen(forms);
  }
}

void AutofillAgent::HidePopup() {
  if (!is_popup_possibly_visible_)
    return;
  is_popup_possibly_visible_ = false;
  is_generation_popup_possibly_visible_ = false;

  // The keyboard accessory has a separate, more complex hiding logic.
  if (IsKeyboardAccessoryEnabled())
    return;

  GetAutofillDriver()->HidePopup();
}

void AutofillAgent::DidAssociateFormControlsDynamically() {
  // If the control flow is here than the document was at least loaded. The
  // whole page doesn't have to be loaded.
  ProcessForms();
  password_autofill_agent_->OnDynamicFormsSeen();
}

void AutofillAgent::DidCompleteFocusChangeInFrame() {
  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  WebElement focused_element;
  if (!doc.IsNull())
    focused_element = doc.FocusedElement();

  if (!focused_element.IsNull() && password_autofill_agent_)
    password_autofill_agent_->FocusedNodeHasChanged(focused_element);

  // PasswordGenerationAgent needs to know about focus changes, even if there is
  // no focused element.
  if (password_generation_agent_ &&
      password_generation_agent_->FocusedNodeHasChanged(focused_element)) {
    is_generation_popup_possibly_visible_ = true;
    is_popup_possibly_visible_ = true;
  }

  if (!IsKeyboardAccessoryEnabled() && focus_requires_scroll_)
    HandleFocusChangeComplete();

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode(
    const WebNode& node) {
  DCHECK(!node.IsNull());
  focused_node_was_last_clicked_ = node.Focused();

  if (IsTouchToFillEnabled() || IsKeyboardAccessoryEnabled() ||
      !focus_requires_scroll_) {
    HandleFocusChangeComplete();
  }
}

void AutofillAgent::SelectControlDidChange(
    const WebFormControlElement& element) {
  form_tracker_.SelectControlDidChange(element);
}

void AutofillAgent::SelectFieldOptionsChanged(
    const blink::WebFormControlElement& element) {
  if (!was_last_action_fill_ || element_.IsNull())
    return;

  // Since a change of a select options often come in batches, use a timer
  // to wait for other changes. Stop the timer if it was already running. It
  // will be started again for this change.
  if (on_select_update_timer_.IsRunning())
    on_select_update_timer_.AbandonAndStop();

  // Start the timer to notify the driver that the select field was updated
  // after the options have finished changing,
  on_select_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kWaitTimeForSelectOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::SelectWasUpdated,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

bool AutofillAgent::ShouldSuppressKeyboard(
    const WebFormControlElement& element) {
  // Note: Consider supporting other autofill types in the future as well.
  return password_autofill_agent_->ShouldSuppressKeyboard() ||
         (autofill_assistant_agent_ &&
          autofill_assistant_agent_->ShouldSuppressKeyboard());
}

void AutofillAgent::FormElementReset(const WebFormElement& form) {
  password_autofill_agent_->InformAboutFormClearing(form);
}

void AutofillAgent::PasswordFieldReset(const WebInputElement& element) {
  password_autofill_agent_->InformAboutFieldClearing(element);
}

void AutofillAgent::SelectWasUpdated(
    const blink::WebFormControlElement& element) {
  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the the form was modified dynamically.
  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element, field_data_manager_.get(),
                                            &form, &field) &&
      !field.option_values.empty()) {
    GetAutofillDriver()->SelectFieldOptionsDidChange(form);
  }
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element,
    bool was_focused) {
  last_clicked_form_control_element_for_testing_ = element;
  last_clicked_form_control_element_was_focused_for_testing_ = was_focused;
  was_last_action_fill_ = false;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (!input_element && !form_util::IsTextAreaElement(element))
    return;

  if (IsTouchToFillEnabled())
    password_autofill_agent_->TryToShowTouchToFill(element);

  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  // Show full suggestions when clicking on an already-focused form field.
  options.show_full_suggestion_list = element.IsAutofilled() || was_focused;

  ShowSuggestions(element, options);

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::HandleFocusChangeComplete() {
  WebElement focused_element =
      render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked. Also check
  // to ensure focus is on a field where text can be entered.
  if ((focused_node_was_last_clicked_ || is_screen_reader_enabled_) &&
      !focused_element.IsNull() && focused_element.IsFormControlElement() &&
      (form_util::IsTextInput(blink::ToWebInputElement(&focused_element)) ||
       focused_element.HasHTMLTagName("textarea"))) {
    FormControlElementClicked(focused_element.ToConst<WebFormControlElement>(),
                              was_focused_before_now_);
  }

  was_focused_before_now_ = true;
  focused_node_was_last_clicked_ = false;

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::AjaxSucceeded() {
  form_tracker_.AjaxSucceeded();

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    ElementChangeSource source) {
  if (source == ElementChangeSource::WILL_SEND_SUBMIT_EVENT) {
    // Fire the form submission event to avoid missing submission when web site
    // handles the onsubmit event, this also gets the form before Javascript
    // could change it.
    // We don't clear submitted_forms_ because OnFormSubmitted will normally be
    // invoked afterwards and we don't want to fire the same event twice.
    FireHostSubmitEvents(form, /*known_success=*/false,
                         SubmissionSource::FORM_SUBMISSION);
    ResetLastInteractedElements();
  } else if (source == ElementChangeSource::TEXTFIELD_CHANGED ||
             source == ElementChangeSource::SELECT_CHANGED) {
    // Remember the last form the user interacted with.
    if (!element.Form().IsNull()) {
      UpdateLastInteractedForm(element.Form());
    } else {
      // Remove invisible elements
      WebLocalFrame* frame = render_frame()->GetWebFrame();
      for (auto it = formless_elements_user_edited_.begin();
           it != formless_elements_user_edited_.end();) {
        if (form_util::IsFormControlVisible(frame, *it)) {
          it = formless_elements_user_edited_.erase(it);
        } else {
          ++it;
        }
      }
      formless_elements_user_edited_.insert(
          FieldRendererId(element.UniqueRendererFormControlId()));
      provisionally_saved_form_ = absl::make_optional<FormData>();
      if (!CollectFormlessElements(&provisionally_saved_form_.value())) {
        provisionally_saved_form_.reset();
      } else {
        last_interacted_form_.Reset();
      }
    }

    if (source == ElementChangeSource::TEXTFIELD_CHANGED) {
      OnTextFieldDidChange(*ToWebInputElement(&element));
    } else {
      FormData form;
      FormFieldData field;
      if (FindFormAndFieldForFormControlElement(
              element, field_data_manager_.get(),
              static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                       GetExtractDatalistMask()),
              &form, &field)) {
        GetAutofillDriver()->SelectControlDidChange(form, field, field.bounds);
      }
    }
  }
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnProbablyFormSubmitted() {
  absl::optional<FormData> form_data = GetSubmittedForm();
  if (form_data.has_value()) {
    FireHostSubmitEvents(form_data.value(), /*known_success=*/false,
                         SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  FireHostSubmitEvents(form, /*known_success=*/false,
                       SubmissionSource::FORM_SUBMISSION);
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  // Only handle iframe for FRAME_DETACHED or main frame for
  // SAME_DOCUMENT_NAVIGATION.
  if ((source == SubmissionSource::FRAME_DETACHED &&
       !render_frame()->GetWebFrame()->Parent()) ||
      (source == SubmissionSource::SAME_DOCUMENT_NAVIGATION &&
       render_frame()->GetWebFrame()->Parent())) {
    ResetLastInteractedElements();
    OnFormNoLongerSubmittable();
    SendPotentiallySubmittedFormToBrowser();
    return;
  }

  if (source == SubmissionSource::FRAME_DETACHED) {
    // Should not access the frame because it is now detached. Instead, use
    // |provisionally_saved_form_|.
    if (provisionally_saved_form_.has_value())
      FireHostSubmitEvents(provisionally_saved_form_.value(),
                           /*known_success=*/true, source);
  } else {
    absl::optional<FormData> form_data = GetSubmittedForm();
    if (form_data.has_value())
      FireHostSubmitEvents(form_data.value(), /*known_success=*/true, source);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::AddFormObserver(Observer* observer) {
  form_tracker_.AddObserver(observer);
}

void AutofillAgent::RemoveFormObserver(Observer* observer) {
  form_tracker_.RemoveObserver(observer);
}

void AutofillAgent::TrackAutofilledElement(
    const blink::WebFormControlElement& element) {
  form_tracker_.TrackAutofilledElement(element);
}

absl::optional<FormData> AutofillAgent::GetSubmittedForm() const {
  if (!last_interacted_form_.IsNull()) {
    FormData form;
    if (form_util::ExtractFormData(last_interacted_form_,
                                   *field_data_manager_.get(), &form)) {
      return absl::make_optional(form);
    } else if (provisionally_saved_form_.has_value()) {
      return absl::make_optional(provisionally_saved_form_.value());
    }
  } else if (formless_elements_user_edited_.size() != 0 &&
             !form_util::IsSomeControlElementVisible(
                 render_frame()->GetWebFrame(),
                 formless_elements_user_edited_)) {
    // we check if all the elements the user has interacted with are gone,
    // to decide if submission has occurred, and use the
    // provisionally_saved_form_ saved in OnProvisionallySaveForm() if fail to
    // construct form.
    FormData form;
    if (CollectFormlessElements(&form)) {
      return absl::make_optional(form);
    } else if (provisionally_saved_form_.has_value()) {
      return absl::make_optional(provisionally_saved_form_.value());
    }
  }
  return absl::nullopt;
}

void AutofillAgent::SendPotentiallySubmittedFormToBrowser() {
  GetAutofillDriver()->SetFormToBeProbablySubmitted(GetSubmittedForm());
}

void AutofillAgent::ResetLastInteractedElements() {
  last_interacted_form_.Reset();
  last_clicked_form_control_element_for_testing_.Reset();
  formless_elements_user_edited_.clear();
  provisionally_saved_form_.reset();
}

void AutofillAgent::UpdateLastInteractedForm(blink::WebFormElement form) {
  last_interacted_form_ = form;
  provisionally_saved_form_ = absl::make_optional<FormData>();
  if (!form_util::ExtractFormData(last_interacted_form_,
                                  *field_data_manager_.get(),
                                  &provisionally_saved_form_.value())) {
    provisionally_saved_form_.reset();
  }
}

void AutofillAgent::OnFormNoLongerSubmittable() {
  submitted_forms_.clear();
}

bool AutofillAgent::FindTheUniqueNewVersionOfOldElement(
    const WebVector<WebFormControlElement>& elements,
    bool& potential_match_encountered,
    WebFormControlElement& matching_element,
    const WebFormControlElement& original_element) {
  if (original_element.IsNull())
    return false;

  const auto original_element_section = original_element.AutofillSection();
  for (const WebFormControlElement& current_element : elements) {
    if (current_element.IsFocusable() &&
        original_element.NameForAutofill() ==
            current_element.NameForAutofill()) {
      // If this is the first matching element, or is the first with the right
      // section, this is the best match so far.
      // In other words: bad, then good. => pick good.
      if (!potential_match_encountered ||
          (current_element.AutofillSection() == original_element_section &&
           (matching_element.IsNull() ||
            matching_element.AutofillSection() != original_element_section))) {
        matching_element = current_element;
        potential_match_encountered = true;
      } else if (current_element.AutofillSection() !=
                     original_element_section &&
                 !matching_element.IsNull() &&
                 matching_element.AutofillSection() !=
                     original_element_section) {
        // The so far matching fields are equally bad. Continue the search if
        // none of them have the correct section.
        // In other words: bad, then bad => pick none.
        matching_element.Reset();
      } else if (current_element.AutofillSection() ==
                     original_element_section &&
                 !matching_element.IsNull() &&
                 matching_element.AutofillSection() ==
                     original_element_section) {
        // If two or more fields have the matching name and section, we can't
        // decide. Two equally good fields => fail.
        matching_element.Reset();
        return false;
      }  // For the good, then bad case => keep good. Continue the search.
    }
  }
  return true;
}

// TODO(crbug.com/896689): Update this method to use the unique ids once they
// are implemented.
void AutofillAgent::ReplaceElementIfNowInvalid(const FormData& original_form) {
  // If the document is invalid, bail out.
  if (element_.GetDocument().IsNull())
    return;

  const auto original_element = element_;
  WebFormControlElement matching_element;
  bool potential_match_encountered = false;

  if (original_form.name.empty()) {
    // If the form has no name, check all the forms.
    for (const WebFormElement& form : element_.GetDocument().Forms()) {
      // If finding a unique element is impossible, don't look further.
      if (!FindTheUniqueNewVersionOfOldElement(
              form.GetFormControlElements(), potential_match_encountered,
              matching_element, original_element))
        return;
    }
    // If the element is not found, we should still check for unowned elements.
    if (!matching_element.IsNull()) {
      element_ = matching_element;
      return;
    }
  }

  // If |element_|'s parent form has no elements, |element_| is now invalid
  // and should be updated.
  if (!element_.Form().IsNull() &&
      element_.Form().GetFormControlElements().empty()) {
    return;
  }

  WebFormElement form_element;
  bool form_is_found = false;
  if (!original_form.name.empty()) {
    // Try to find the new version of the form.
    for (const WebFormElement& form : element_.GetDocument().Forms()) {
      if (original_form.name == form.GetName().Utf16() ||
          original_form.name == form.GetAttribute("id").Utf16()) {
        if (!form_is_found)
          form_element = form;
        else  // multiple forms with the matching name.
          return;
      }
    }
  }

  if (form_element.IsNull()) {
    // Could not find the new version of the form, get all the unowned elements.
    std::vector<WebElement> fieldsets;
    WebVector<WebFormControlElement> elements =
        form_util::GetUnownedAutofillableFormFieldElements(
            element_.GetDocument().All(), &fieldsets);
    // If a unique match was found.
    if (FindTheUniqueNewVersionOfOldElement(
            elements, potential_match_encountered, matching_element,
            original_element) &&
        !matching_element.IsNull()) {
      element_ = matching_element;
    }
    return;
  }
  // This is the case for owned fields that belong to the right named form.
  // Get all the elements of the new version of the form.
  // If a unique match was found.
  if (FindTheUniqueNewVersionOfOldElement(form_element.GetFormControlElements(),
                                          potential_match_encountered,
                                          matching_element, original_element) &&
      !matching_element.IsNull()) {
    element_ = matching_element;
  }
}

const mojo::AssociatedRemote<mojom::AutofillDriver>&
AutofillAgent::GetAutofillDriver() {
  if (!autofill_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_driver_);
  }

  return autofill_driver_;
}

const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
AutofillAgent::GetPasswordManagerDriver() {
  DCHECK(password_autofill_agent_);
  return password_autofill_agent_->GetPasswordManagerDriver();
}

}  // namespace autofill
