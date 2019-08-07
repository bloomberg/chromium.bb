// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_generation_agent.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;

namespace autofill {

namespace {

using autofill::form_util::GetTextDirectionForElement;
using Logger = autofill::SavePasswordProgressLogger;

// Returns pairs of |PasswordForm| and corresponding |WebFormElement| for all
// <form>s in the frame and for unowned <input>s. The method doesn't filter out
// invalid |PasswordForm|s.
std::vector<std::pair<std::unique_ptr<PasswordForm>, WebFormElement>>
GetAllPasswordFormsInFrame(PasswordAutofillAgent* password_agent,
                           WebLocalFrame* web_frame) {
  blink::WebVector<WebFormElement> web_forms;
  web_frame->GetDocument().Forms(web_forms);
  std::vector<std::pair<std::unique_ptr<PasswordForm>, WebFormElement>>
      all_forms;
  for (const WebFormElement& web_form : web_forms) {
    all_forms.emplace_back(std::make_pair(
        password_agent->GetPasswordFormFromWebForm(web_form), web_form));
  }
  all_forms.emplace_back(
      std::make_pair(password_agent->GetPasswordFormFromUnownedInputElements(),
                     WebFormElement()));
  return all_forms;
}

// Returns true if we think that this form is for account creation. Password
// field(s) of the form are pushed back to |passwords|.
bool GetAccountCreationPasswordFields(
    const std::vector<WebFormControlElement>& control_elements,
    std::vector<WebInputElement>* passwords) {
  for (const auto& control_element : control_elements) {
    const WebInputElement* input_element = ToWebInputElement(&control_element);
    if (input_element && input_element->IsTextField() &&
        input_element->IsPasswordFieldForAutofill()) {
      passwords->push_back(*input_element);
    }
  }
  return !passwords->empty();
}

// Returns the renderer id of the next password field in |control_elements|
// after |new_password|. This field is likely to be the confirmation field.
// Returns FormFieldData::kNotSetFormControlRendererId if there is no such
// field.
uint32_t FindConfirmationPasswordFieldId(
    const std::vector<WebFormControlElement>& control_elements,
    const WebFormControlElement& new_password) {
  auto iter =
      std::find(control_elements.begin(), control_elements.end(), new_password);

  if (iter == control_elements.end())
    return FormFieldData::kNotSetFormControlRendererId;

  ++iter;
  for (; iter != control_elements.end(); ++iter) {
    const WebInputElement* input_element = ToWebInputElement(&(*iter));
    if (input_element && input_element->IsPasswordFieldForAutofill())
      return input_element->UniqueRendererFormControlId();
  }
  return FormFieldData::kNotSetFormControlRendererId;
}

bool ContainsURL(const std::vector<GURL>& urls, const GURL& url) {
  return base::ContainsValue(urls, url);
}

// Calculates the signature of |form| and searches it in |forms|.
const PasswordFormGenerationData* FindFormGenerationData(
    const std::vector<PasswordFormGenerationData>& forms,
    const PasswordForm& form) {
  FormSignature form_signature = CalculateFormSignature(form.form_data);
  for (const auto& form_it : forms) {
    if (form_it.form_signature == form_signature)
      return &form_it;
  }
  return nullptr;
}

// Returns a vector of up to 2 password fields with autocomplete attribute set
// to "new-password". These will be filled with the generated password.
std::vector<WebInputElement> FindNewPasswordElementsMarkedBySite(
    const std::vector<WebInputElement>& all_password_elements) {
  std::vector<WebInputElement> passwords;

  auto is_new_password_field = [](const WebInputElement& element) {
    return AutocompleteFlagForElement(element) ==
           AutocompleteFlag::NEW_PASSWORD;
  };

  auto field_iter =
      std::find_if(all_password_elements.begin(), all_password_elements.end(),
                   is_new_password_field);
  if (field_iter != all_password_elements.end()) {
    passwords.push_back(*field_iter++);
    field_iter = std::find_if(field_iter, all_password_elements.end(),
                              is_new_password_field);
    if (field_iter != all_password_elements.end())
      passwords.push_back(*field_iter);
  }

  return passwords;
}

// Returns a vector of up to 2 password fields into which Chrome should fill the
// generated password. It assumes that |field_signature| describes the field
// where Chrome shows the password generation prompt.
std::vector<WebInputElement> FindPasswordElementsForGeneration(
    const std::vector<WebInputElement>& all_password_elements,
    const PasswordFormGenerationData& generation_data) {
  auto generation_field_iter = all_password_elements.end();
  auto confirmation_field_iter = all_password_elements.end();
  for (auto iter = all_password_elements.begin();
       iter != all_password_elements.end(); ++iter) {
    const WebInputElement& input = *iter;
    FieldSignature signature = CalculateFieldSignatureByNameAndType(
        input.NameForAutofill().Utf16(),
        input.FormControlTypeForAutofill().Utf8());
    if (signature == generation_data.field_signature) {
      generation_field_iter = iter;
    } else if (generation_data.confirmation_field_signature &&
               signature == *generation_data.confirmation_field_signature) {
      confirmation_field_iter = iter;
    }
  }

  std::vector<WebInputElement> passwords;
  if (generation_field_iter != all_password_elements.end()) {
    passwords.push_back(*generation_field_iter);

    if (confirmation_field_iter == all_password_elements.end())
      confirmation_field_iter = generation_field_iter + 1;
    if (confirmation_field_iter != all_password_elements.end())
      passwords.push_back(*confirmation_field_iter);
  }
  return passwords;
}

void CopyElementValueToOtherInputElements(
    const WebInputElement* element,
    std::vector<WebInputElement>* elements) {
  for (WebInputElement& it : *elements) {
    if (*element != it) {
      it.SetAutofillValue(element->Value());
    }
    it.SetAutofillState(WebAutofillState::kAutofilled);
  }
}

}  // namespace

// Contains information about a form for which generation is possible.
struct PasswordGenerationAgent::AccountCreationFormData {
  PasswordForm form;
  std::vector<blink::WebInputElement> password_elements;

  AccountCreationFormData(PasswordForm password_form,
                          std::vector<blink::WebInputElement> passwords)
      : form(std::move(password_form)),
        password_elements(std::move(passwords)) {}
  AccountCreationFormData(AccountCreationFormData&& rhs) = default;

  ~AccountCreationFormData() = default;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationFormData);
};

// Contains information about generation status for an element for the
// lifetime of the possible interaction.
struct PasswordGenerationAgent::GenerationItemInfo {
  GenerationItemInfo(const AccountCreationFormData& creation_form_data,
                     const blink::WebInputElement& generation_element)
      : generation_element_(generation_element) {
    form_ = creation_form_data.form;
    password_elements_ = creation_form_data.password_elements;
  }
  GenerationItemInfo(blink::WebInputElement generation_element,
                     PasswordForm form,
                     std::vector<blink::WebInputElement> password_elements)
      : generation_element_(std::move(generation_element)),
        form_(std::move(form)),
        password_elements_(std::move(password_elements)) {}
  ~GenerationItemInfo() = default;

  // Element where we want to trigger password generation UI.
  blink::WebInputElement generation_element_;

  // Password form for the generation element.
  PasswordForm form_;

  // All the password elements in the form.
  std::vector<blink::WebInputElement> password_elements_;

  // If the password field at |generation_element_| contains a generated
  // password.
  bool password_is_generated_ = false;

  // True if the last password generation was manually triggered.
  bool is_manually_triggered_ = false;

  // True if a password was generated and the user edited it. Used for UMA
  // stats.
  bool password_edited_ = false;

  // True if the generation popup was shown during this navigation. Used to
  // track UMA stats per page visit rather than per display, since the former
  // is more interesting.
  // TODO(crbug.com/845458): Remove this or change the description of the
  // logged event as calling AutomaticgenerationStatusChanged will no longer
  // imply that a popup is shown. This could instead be logged with the
  // metrics collected on the browser process.
  bool generation_popup_shown_ = false;

  // True if the editing popup was shown during this navigation. Used to track
  // UMA stats per page rather than per display, since the former is more
  // interesting.
  bool editing_popup_shown_ = false;

  // True when PasswordGenerationAgent updates other password fields on the page
  // due to the generated password being edited. It's used to suppress the fake
  // blur events coming from there.
  bool updating_other_password_fileds_ = false;

  DISALLOW_COPY_AND_ASSIGN(GenerationItemInfo);
};

PasswordGenerationAgent::PasswordGenerationAgent(
    content::RenderFrame* render_frame,
    PasswordAutofillAgent* password_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      enabled_(password_generation::IsPasswordGenerationEnabled()),
      mark_generation_element_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kShowAutofillSignatures)),
      password_agent_(password_agent),
      binding_(this) {
  LogBoolean(Logger::STRING_GENERATION_RENDERER_ENABLED, enabled_);
  registry->AddInterface(base::BindRepeating(
      &PasswordGenerationAgent::BindRequest, base::Unretained(this)));
  password_agent_->SetPasswordGenerationAgent(this);
}

PasswordGenerationAgent::~PasswordGenerationAgent() = default;

void PasswordGenerationAgent::BindRequest(
    mojom::PasswordGenerationAgentAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void PasswordGenerationAgent::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;
  // Update stats for main frame navigation.
  if (!render_frame()->GetWebFrame()->Parent()) {
    // Log statistics after navigation so that we only log once per page.
    if (automatic_generation_form_data_ &&
        !automatic_generation_form_data_->password_elements.empty()) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::SIGN_UP_DETECTED);
    } else {
      password_generation::LogPasswordGenerationEvent(
          password_generation::NO_SIGN_UP_DETECTED);
    }
    if (current_generation_item_) {
      if (current_generation_item_->password_edited_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::PASSWORD_EDITED);
      }
      if (current_generation_item_->generation_popup_shown_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::GENERATION_POPUP_SHOWN);
      }
      if (current_generation_item_->editing_popup_shown_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::EDITING_POPUP_SHOWN);
      }
    }
  }
  possible_account_creation_forms_.clear();
  not_blacklisted_password_form_origins_.clear();
  generation_enabled_forms_.clear();
  automatic_generation_form_data_.reset();
  automatic_generation_element_.Reset();
  current_generation_item_.reset();
  last_focused_password_element_.Reset();
  generation_enabled_fields_.clear();
}

void PasswordGenerationAgent::DidChangeScrollOffset() {
  if (!current_generation_item_)
    return;
  GetPasswordGenerationDriver()->FrameWasScrolled();
}

void PasswordGenerationAgent::DidFinishDocumentLoad() {
  FindPossibleGenerationForm();
}

void PasswordGenerationAgent::DidFinishLoad() {
  // Since forms on some sites are available only at this event (but not at
  // DidFinishDocumentLoad), again call FindPossibleGenerationForm to detect
  // these forms (crbug.com/617893).
  FindPossibleGenerationForm();
}

void PasswordGenerationAgent::OnDestruct() {
  binding_.Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PasswordGenerationAgent::OnDynamicFormsSeen() {
  FindPossibleGenerationForm();
}

void PasswordGenerationAgent::OnFieldAutofilled(
    const WebInputElement& password_element) {
  if (current_generation_item_ &&
      current_generation_item_->password_is_generated_ &&
      current_generation_item_->generation_element_ == password_element) {
    password_generation::LogPasswordGenerationEvent(
        password_generation::PASSWORD_DELETED_BY_AUTOFILLING);
    PasswordNoLongerGenerated();
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }
}

bool PasswordGenerationAgent::ShouldIgnoreBlur() const {
  return current_generation_item_ &&
         current_generation_item_->updating_other_password_fileds_;
}

void PasswordGenerationAgent::FindPossibleGenerationForm() {
  if (!enabled_ || !render_frame())
    return;

  // We don't want to generate passwords if the browser won't store or sync
  // them.
  if (!ShouldAnalyzeDocument())
    return;

  // If we have already found a signup form for this page, no need to continue.
  if (automatic_generation_form_data_)
    return;

  WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  std::vector<std::pair<std::unique_ptr<PasswordForm>, WebFormElement>>
      all_password_forms =
          GetAllPasswordFormsInFrame(password_agent_, web_frame);
  for (auto& form : all_password_forms) {
    PasswordForm* password_form = form.first.get();
    // If we can't get a valid PasswordForm, we skip this form because the
    // the password won't get saved even if we generate it.
    if (!password_form) {
      LogMessage(Logger::STRING_GENERATION_RENDERER_INVALID_PASSWORD_FORM);
      continue;
    }

    // Do not generate password for GAIA since it is used to retrieve the
    // generated paswords.
    GURL realm(password_form->signon_realm);
    if (realm == GaiaUrls::GetInstance()->gaia_login_form_realm())
      continue;

    std::vector<WebInputElement> passwords;
    const WebFormElement& web_form = form.second;
    if (GetAccountCreationPasswordFields(
            web_form.IsNull()
                ? form_util::GetUnownedFormFieldElements(
                      web_frame->GetDocument().All(), nullptr)
                : form_util::ExtractAutofillableElementsInForm(web_form),
            &passwords)) {
      possible_account_creation_forms_.emplace_back(std::move(*form.first),
                                                    std::move(passwords));
    }
  }

  if (!possible_account_creation_forms_.empty()) {
    LogNumber(
        Logger::STRING_GENERATION_RENDERER_POSSIBLE_ACCOUNT_CREATION_FORMS,
        possible_account_creation_forms_.size());
    DetermineGenerationElement();
  }
}

bool PasswordGenerationAgent::ShouldAnalyzeDocument() {
  // Make sure that this frame is allowed to use password manager. Generating a
  // password that can't be saved is a bad idea.
  if (!render_frame() || !password_agent_->FrameCanAccessPasswordManager()) {
    LogMessage(Logger::STRING_GENERATION_RENDERER_NO_PASSWORD_MANAGER_ACCESS);
    return false;
  }

  return true;
}

void PasswordGenerationAgent::FormNotBlacklisted(const PasswordForm& form) {
  not_blacklisted_password_form_origins_.push_back(form.origin);
  DetermineGenerationElement();
}

void PasswordGenerationAgent::GeneratedPasswordAccepted(
    const base::string16& password) {
  // static cast is workaround for linker error.
  DCHECK_LE(static_cast<size_t>(kMinimumLengthForEditedPassword),
            password.size());
  DCHECK(current_generation_item_);
  current_generation_item_->password_is_generated_ = true;
  current_generation_item_->password_edited_ = false;
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_ACCEPTED);
  LogMessage(Logger::STRING_GENERATION_RENDERER_GENERATED_PASSWORD_ACCEPTED);
  for (auto& password_element : current_generation_item_->password_elements_) {
    base::AutoReset<bool> auto_reset_update_confirmation_password(
        &current_generation_item_->updating_other_password_fileds_, true);
    password_element.SetAutofillValue(blink::WebString::FromUTF16(password));
    // setAutofillValue() above may have resulted in JavaScript closing the
    // frame.
    if (!render_frame())
      return;
    password_element.SetAutofillState(WebAutofillState::kAutofilled);
    // Advance focus to the next input field. We assume password fields in
    // an account creation form are always adjacent.
    render_frame()->GetRenderView()->GetWebView()->AdvanceFocus(false);
  }
  std::unique_ptr<PasswordForm> presaved_form(CreatePasswordFormToPresave());
  if (presaved_form) {
    DCHECK_NE(base::string16(), presaved_form->password_value);
    GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
  }

  // Call UpdateStateForTextChange after the corresponding PasswordFormManager
  // is notified that the password was generated.
  for (auto& password_element : current_generation_item_->password_elements_) {
    // Needed to notify password_autofill_agent that the content of the field
    // has changed. Without this we will overwrite the generated
    // password with an Autofilled password when saving.
    // https://crbug.com/493455
    password_agent_->UpdateStateForTextChange(password_element);
  }
}

std::unique_ptr<PasswordForm>
PasswordGenerationAgent::CreatePasswordFormToPresave() {
  DCHECK(current_generation_item_);
  DCHECK(!current_generation_item_->generation_element_.IsNull());
  // Since the form for presaving should match a form in the browser, create it
  // with the same algorithm (to match html attributes, action, etc.), but
  // change username and password values.
  std::unique_ptr<PasswordForm> password_form;
  if (!current_generation_item_->generation_element_.Form().IsNull()) {
    password_form = password_agent_->GetPasswordFormFromWebForm(
        current_generation_item_->generation_element_.Form());
  } else {
    password_form = password_agent_->GetPasswordFormFromUnownedInputElements();
  }
  if (password_form) {
    password_form->type = PasswordForm::Type::kGenerated;
    password_form->password_value =
        current_generation_item_->generation_element_.Value().Utf16();
  }

  return password_form;
}

void PasswordGenerationAgent::FoundFormsEligibleForGeneration(
    const std::vector<PasswordFormGenerationData>& forms) {
  generation_enabled_forms_.insert(generation_enabled_forms_.end(),
                                   forms.begin(), forms.end());
  DetermineGenerationElement();
}

void PasswordGenerationAgent::FoundFormEligibleForGeneration(
    const NewPasswordFormGenerationData& form) {
  generation_enabled_fields_[form.new_password_renderer_id] = form;

  if (mark_generation_element_) {
    // Mark the input element with renderer id |form.new_password_renderer_id|.
    if (!render_frame())
      return;
    WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
    if (doc.IsNull())
      return;
    WebFormControlElement new_password_input =
        form_util::FindFormControlElementsByUniqueRendererId(
            doc, form.new_password_renderer_id);
    if (!new_password_input.IsNull())
      new_password_input.SetAttribute("password_creation_field", "1");
  }
}

void PasswordGenerationAgent::UserTriggeredGeneratePassword(
    UserTriggeredGeneratePasswordCallback callback) {
  if (SetUpUserTriggeredGeneration()) {
    LogMessage(Logger::STRING_GENERATION_RENDERER_SHOW_MANUAL_GENERATION_POPUP);
    autofill::password_generation::PasswordGenerationUIData
        password_generation_ui_data(
            render_frame()->GetRenderView()->ElementBoundsInWindow(
                current_generation_item_->generation_element_),
            current_generation_item_->generation_element_.MaxLength(),
            current_generation_item_->generation_element_.NameForAutofill()
                .Utf16(),
            GetTextDirectionForElement(
                current_generation_item_->generation_element_),
            current_generation_item_->form_);
    std::move(callback).Run(std::move(password_generation_ui_data));
    current_generation_item_->generation_popup_shown_ = true;
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

void PasswordGenerationAgent::DetermineGenerationElement() {
  if (automatic_generation_form_data_) {
    LogMessage(Logger::STRING_GENERATION_RENDERER_FORM_ALREADY_FOUND);
    return;
  }

  // Make sure local heuristics have identified a possible account creation
  // form.
  if (possible_account_creation_forms_.empty()) {
    LogMessage(Logger::STRING_GENERATION_RENDERER_NO_POSSIBLE_CREATION_FORMS);
    return;
  }

  // Note that no messages will be sent if this feature is disabled
  // (e.g. password saving is disabled).
  for (auto& possible_form_data : possible_account_creation_forms_) {
    PasswordForm* possible_password_form = &possible_form_data.form;
    std::vector<WebInputElement> password_elements;
    if (!ContainsURL(not_blacklisted_password_form_origins_,
                     possible_password_form->origin)) {
      LogMessage(Logger::STRING_GENERATION_RENDERER_NOT_BLACKLISTED);
      continue;
    } else {
      const PasswordFormGenerationData* generation_data =
          FindFormGenerationData(generation_enabled_forms_,
                                 *possible_password_form);
      if (generation_data) {
        password_elements = FindPasswordElementsForGeneration(
            possible_form_data.password_elements, *generation_data);
      } else {
        if (!possible_password_form->new_password_marked_by_site) {
          LogMessage(Logger::STRING_GENERATION_RENDERER_NO_SERVER_SIGNAL);
          continue;
        }

        LogMessage(Logger::STRING_GENERATION_RENDERER_AUTOCOMPLETE_ATTRIBUTE);
        password_generation::LogPasswordGenerationEvent(
            password_generation::AUTOCOMPLETE_ATTRIBUTES_ENABLED_GENERATION);

        password_elements = FindNewPasswordElementsMarkedBySite(
            possible_form_data.password_elements);
      }
    }

    LogMessage(Logger::STRING_GENERATION_RENDERER_ELIGIBLE_FORM_FOUND);
    if (password_elements.empty()) {
      // It might be if JavaScript changes field names.
      LogMessage(Logger::STRING_GENERATION_RENDERER_NO_FIELD_FOUND);
      return;
    }

    automatic_generation_form_data_.reset(new AccountCreationFormData(
        possible_form_data.form, std::move(password_elements)));
    automatic_generation_element_ =
        automatic_generation_form_data_->password_elements[0];
    if (mark_generation_element_)
      automatic_generation_element_.SetAttribute("password_creation_field",
                                                 "1");
    automatic_generation_element_.SetAttribute("aria-autocomplete", "list");
    password_generation::LogPasswordGenerationEvent(
        password_generation::GENERATION_AVAILABLE);
    possible_account_creation_forms_.clear();
    if (!current_generation_item_) {
      // If the manual generation hasn't started, set
      // |automatic_generation_element_| as the current generation field.
      current_generation_item_.reset(new GenerationItemInfo(
          *automatic_generation_form_data_, automatic_generation_element_));
    }
    GetPasswordGenerationDriver()->GenerationAvailableForForm(
        automatic_generation_form_data_->form);
    return;
  }
}

bool PasswordGenerationAgent::SetUpUserTriggeredGeneration() {
  if (last_focused_password_element_.IsNull() || !render_frame())
    return false;

  uint32_t last_focused_password_element_id =
      last_focused_password_element_.UniqueRendererFormControlId();

  bool is_automatic_generation_available = base::ContainsKey(
      generation_enabled_fields_, last_focused_password_element_id);

  if (!is_automatic_generation_available) {
    WebFormElement form = last_focused_password_element_.Form();
    std::vector<WebFormControlElement> control_elements;
    if (!form.IsNull()) {
      control_elements = form_util::ExtractAutofillableElementsInForm(form);
    } else {
      const WebLocalFrame& frame = *render_frame()->GetWebFrame();
      blink::WebDocument doc = frame.GetDocument();
      if (doc.IsNull())
        return false;
      control_elements =
          form_util::GetUnownedFormFieldElements(doc.All(), nullptr);
    }

    MaybeCreateCurrentGenerationItem(
        last_focused_password_element_,
        FindConfirmationPasswordFieldId(control_elements,
                                        last_focused_password_element_));
  } else {
    auto it = generation_enabled_fields_.find(last_focused_password_element_id);
    MaybeCreateCurrentGenerationItem(
        last_focused_password_element_,
        it->second.confirmation_password_renderer_id);
  }

  if (!current_generation_item_)
    return false;

  if (current_generation_item_->generation_element_ !=
      last_focused_password_element_) {
    return false;
  }

  // Automatic generation depends on whether the new parser is on because the
  // new parser sends now information to the renderer about fields for
  // generation. In case when the new old parser is used the old path for
  // automatic generation is used. So detecting which automatic generation
  // depends on which parser is used.
  // TODO(https://crbug.com/831123): Remove this variable when the old parser is
  // gone.
  bool automatic_generation_available_with_the_old_parser =
      last_focused_password_element_ == automatic_generation_element_;

  current_generation_item_->is_manually_triggered_ =
      !is_automatic_generation_available &&
      !automatic_generation_available_with_the_old_parser;
  return true;
}

bool PasswordGenerationAgent::FocusedNodeHasChanged(
    const blink::WebNode& node) {
  if (node.IsNull() || !node.IsElementNode()) {
    return false;
  }

  const blink::WebElement web_element = node.ToConst<blink::WebElement>();
  if (!web_element.GetDocument().GetFrame()) {
    return false;
  }

  const WebInputElement* element = ToWebInputElement(&web_element);
  if (!element)
    return false;

  if (element->IsPasswordFieldForAutofill())
    last_focused_password_element_ = *element;

  auto it =
      generation_enabled_fields_.find(element->UniqueRendererFormControlId());
  if (it != generation_enabled_fields_.end()) {
    MaybeCreateCurrentGenerationItem(
        *element, it->second.confirmation_password_renderer_id);
  }

  if (!current_generation_item_ ||
      *element != current_generation_item_->generation_element_) {
    return false;
  }

  if (current_generation_item_->password_is_generated_) {
    if (current_generation_item_->generation_element_.Value().length() <
        kMinimumLengthForEditedPassword) {
      PasswordNoLongerGenerated();
      MaybeOfferAutomaticGeneration();
      if (current_generation_item_->generation_element_.Value().IsEmpty())
        current_generation_item_->generation_element_.SetShouldRevealPassword(
            false);
    } else {
      current_generation_item_->generation_element_.SetShouldRevealPassword(
          true);
      ShowEditingPopup();
    }
    return true;
  }

  // Assume that if the password field has less than
  // |kMaximumCharsForGenerationOffer| characters then the user is not finished
  // typing their password and display the password suggestion.
  if (!element->IsReadOnly() && element->IsEnabled() &&
      element->Value().length() <= kMaximumCharsForGenerationOffer) {
    MaybeOfferAutomaticGeneration();
    return true;
  }

  return false;
}

void PasswordGenerationAgent::DidEndTextFieldEditing(
    const blink::WebInputElement& element) {
  if (!element.IsNull() && current_generation_item_ &&
      element == current_generation_item_->generation_element_) {
    GetPasswordGenerationDriver()->GenerationElementLostFocus();
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }
}

bool PasswordGenerationAgent::TextDidChangeInTextField(
    const WebInputElement& element) {
  if (!(current_generation_item_ &&
        current_generation_item_->generation_element_ == element)) {
    // Presave the username if it has been changed.
    if (current_generation_item_ &&
        current_generation_item_->password_is_generated_ && !element.IsNull() &&
        element.Form() ==
            current_generation_item_->generation_element_.Form()) {
      std::unique_ptr<PasswordForm> presaved_form(
          CreatePasswordFormToPresave());
      if (presaved_form)
        GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
    }
    return false;
  }

  if (element.Value().IsEmpty()) {
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }

  if (!current_generation_item_->password_is_generated_ &&
      element.Value().length() > kMaximumCharsForGenerationOffer) {
    // User has rejected the feature and has started typing a password.
    GenerationRejectedByTyping();
  } else {
    const bool leave_editing_state =
        current_generation_item_->password_is_generated_ &&
        element.Value().length() < kMinimumLengthForEditedPassword;
    if (!current_generation_item_->password_is_generated_ ||
        leave_editing_state) {
      // The call may pop up a generation prompt, replacing the editing prompt
      // if it was previously shown.
      MaybeOfferAutomaticGeneration();
    }
    if (leave_editing_state) {
      // Tell the browser that the state isn't "editing" anymore. The browser
      // should hide the editing prompt if it wasn't replaced above.
      PasswordNoLongerGenerated();
    } else if (current_generation_item_->password_is_generated_) {
      current_generation_item_->password_edited_ = true;
      base::AutoReset<bool> auto_reset_update_confirmation_password(
          &current_generation_item_->updating_other_password_fileds_, true);
      // Mirror edits to any confirmation password fields.
      CopyElementValueToOtherInputElements(
          &element, &current_generation_item_->password_elements_);
      std::unique_ptr<PasswordForm> presaved_form(
          CreatePasswordFormToPresave());
      if (presaved_form) {
        GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
      }
    }
  }
  return true;
}

void PasswordGenerationAgent::MaybeOfferAutomaticGeneration() {
  // TODO(crbug.com/852309): Add this check to the generation element class.
  if (!current_generation_item_->is_manually_triggered_) {
    AutomaticGenerationAvailable();
  }
}

void PasswordGenerationAgent::AutomaticGenerationAvailable() {
  if (!render_frame())
    return;
  DCHECK(current_generation_item_);
  DCHECK(!current_generation_item_->generation_element_.IsNull());
  LogMessage(Logger::STRING_GENERATION_RENDERER_AUTOMATIC_GENERATION_AVAILABLE);
  autofill::password_generation::PasswordGenerationUIData
      password_generation_ui_data(
          render_frame()->GetRenderView()->ElementBoundsInWindow(
              current_generation_item_->generation_element_),
          current_generation_item_->generation_element_.MaxLength(),
          current_generation_item_->generation_element_.NameForAutofill()
              .Utf16(),
          GetTextDirectionForElement(
              current_generation_item_->generation_element_),
          current_generation_item_->form_);
  current_generation_item_->generation_popup_shown_ = true;
  GetPasswordGenerationDriver()->AutomaticGenerationAvailable(
      password_generation_ui_data);
}

void PasswordGenerationAgent::ShowEditingPopup() {
  if (!render_frame())
    return;
  GetPasswordGenerationDriver()->ShowPasswordEditingPopup(
      render_frame()->GetRenderView()->ElementBoundsInWindow(
          current_generation_item_->generation_element_),
      *CreatePasswordFormToPresave());
  current_generation_item_->editing_popup_shown_ = true;
}

void PasswordGenerationAgent::GenerationRejectedByTyping() {
  GetPasswordGenerationDriver()->PasswordGenerationRejectedByTyping();
}

void PasswordGenerationAgent::PasswordNoLongerGenerated() {
  DCHECK(current_generation_item_);
  DCHECK(current_generation_item_->password_is_generated_);
  // Do not treat the password as generated, either here or in the browser.
  current_generation_item_->password_is_generated_ = false;
  current_generation_item_->password_edited_ = false;
  for (WebInputElement& password : current_generation_item_->password_elements_)
    password.SetAutofillState(WebAutofillState::kNotFilled);
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_DELETED);
  // Clear all other password fields.
  for (WebInputElement& element :
       current_generation_item_->password_elements_) {
    base::AutoReset<bool> auto_reset_update_confirmation_password(
        &current_generation_item_->updating_other_password_fileds_, true);
    if (current_generation_item_->generation_element_ != element)
      element.SetAutofillValue(blink::WebString());
  }
  std::unique_ptr<PasswordForm> presaved_form(CreatePasswordFormToPresave());
  if (presaved_form)
    GetPasswordGenerationDriver()->PasswordNoLongerGenerated(*presaved_form);
}

void PasswordGenerationAgent::MaybeCreateCurrentGenerationItem(
    WebInputElement element,
    uint32_t confirmation_password_renderer_id) {
  // Do not create |current_generation_item_| if it already is created for
  // |element| or the user accepted generated password. So if the user accepted
  // the generated password, generation is not offered on any other field.
  if (current_generation_item_ &&
      (current_generation_item_->generation_element_ == element ||
       current_generation_item_->password_is_generated_))
    return;

  std::unique_ptr<PasswordForm> password_form =
      element.Form().IsNull()
          ? password_agent_->GetPasswordFormFromUnownedInputElements()
          : password_agent_->GetPasswordFormFromWebForm(element.Form());

  if (!password_form)
    return;

  std::vector<blink::WebInputElement> passwords = {element};

  WebFormControlElement confirmation_password =
      form_util::FindFormControlElementsByUniqueRendererId(
          element.GetDocument(), confirmation_password_renderer_id);

  if (!confirmation_password.IsNull()) {
    WebInputElement* input = ToWebInputElement(&confirmation_password);
    if (input)
      passwords.push_back(*input);
  }

  current_generation_item_.reset(new GenerationItemInfo(
      element, std::move(*password_form), std::move(passwords)));

  element.SetAttribute("aria-autocomplete", "list");
}

const mojom::PasswordManagerDriverAssociatedPtr&
PasswordGenerationAgent::GetPasswordManagerDriver() {
  DCHECK(password_agent_);
  return password_agent_->GetPasswordManagerDriver();
}

const mojom::PasswordGenerationDriverAssociatedPtr&
PasswordGenerationAgent::GetPasswordGenerationDriver() {
  if (!password_generation_client_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_generation_client_);
  }

  return password_generation_client_;
}

void PasswordGenerationAgent::LogMessage(Logger::StringID message_id) {
  if (!password_agent_->logging_state_active())
    return;
  RendererSavePasswordProgressLogger logger(GetPasswordManagerDriver().get());
  logger.LogMessage(message_id);
}

void PasswordGenerationAgent::LogBoolean(Logger::StringID message_id,
                                         bool truth_value) {
  if (!password_agent_->logging_state_active())
    return;
  RendererSavePasswordProgressLogger logger(GetPasswordManagerDriver().get());
  logger.LogBoolean(message_id, truth_value);
}

void PasswordGenerationAgent::LogNumber(Logger::StringID message_id,
                                        int number) {
  if (!password_agent_->logging_state_active())
    return;
  RendererSavePasswordProgressLogger logger(GetPasswordManagerDriver().get());
  logger.LogNumber(message_id, number);
}

}  // namespace autofill
