// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/bad_message.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofillable_data.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

namespace autofill {

namespace {

bool ShouldEnableHeavyFormDataScraping(const version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return true;
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    ContentAutofillRouter* autofill_router,
    AutofillManager::AutofillDownloadManagerState enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback)
    : render_frame_host_(render_frame_host),
      autofill_router_(autofill_router),
      browser_autofill_manager_(nullptr),
      log_manager_(client->GetLogManager()) {
  // AutofillManager isn't used if provider is valid, Autofill provider is
  // currently used by Android WebView only.
  if (autofill_manager_factory_callback) {
    autofill_manager_ = autofill_manager_factory_callback.Run(
        this, client, app_locale, enable_download_manager);
    GetAutofillAgent()->SetUserGestureRequired(false);
    GetAutofillAgent()->SetSecureContextRequired(true);
    GetAutofillAgent()->SetFocusRequiresScroll(false);
    GetAutofillAgent()->SetQueryPasswordSuggestion(true);
  } else {
    SetBrowserAutofillManager(std::make_unique<BrowserAutofillManager>(
        this, client, app_locale, enable_download_manager));
  }
  if (client && ShouldEnableHeavyFormDataScraping(client->GetChannel())) {
    GetAutofillAgent()->EnableHeavyFormDataScraping();
  }
}

ContentAutofillDriver::~ContentAutofillDriver() {
  if (autofill_router_)  // Can be nullptr only in tests.
    autofill_router_->UnregisterDriver(this);
}

void ContentAutofillDriver::TriggerReparse() {
  GetAutofillAgent()->TriggerReparse();
}

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->DriverForFrame(render_frame_host) : nullptr;
}

void ContentAutofillDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

// TODO(https://crbug.com/1225171): Consider renaming this function to
// |IsOffTheRecord| if off-the-record Guest mode is not deprecated.
bool ContentAutofillDriver::IsIncognito() const {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->IsOffTheRecord();
}

bool ContentAutofillDriver::IsInMainFrame() const {
  return render_frame_host_->GetParent() == nullptr;
}

bool ContentAutofillDriver::IsPrerendering() const {
  return render_frame_host_->GetLifecycleState() ==
         content::RenderFrameHost::LifecycleState::kPrerendering;
}

bool ContentAutofillDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for inactive RenderFrameHost. Here it is safe to
  // ignore the calls from inactive RFH as the renderer is not expecting a reply
  // and it doesn't lead to browser-renderer consistency issues.
  return render_frame_host_->IsActive();
}

ui::AXTreeID ContentAutofillDriver::GetAxTreeId() const {
  return render_frame_host_->GetAXTreeID();
}

scoped_refptr<network::SharedURLLoaderFactory>
ContentAutofillDriver::GetURLLoaderFactory() {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != nullptr;
}

webauthn::InternalAuthenticator*
ContentAutofillDriver::GetOrCreateCreditCardInternalAuthenticator() {
  if (!authenticator_impl_ && browser_autofill_manager_ &&
      browser_autofill_manager_->client()) {
    authenticator_impl_ =
        browser_autofill_manager_->client()
            ->CreateCreditCardInternalAuthenticator(render_frame_host_);
  }
  return authenticator_impl_.get();
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is shown, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (browser_autofill_manager_ &&
      browser_autofill_manager_->ShouldClearPreviewedForm()) {
    RendererShouldClearPreviewedForm();
  }
}

gfx::RectF ContentAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return bounding_box;

  gfx::PointF orig_point(bounding_box.x(), bounding_box.y());
  gfx::PointF transformed_point =
      view->TransformPointToRootCoordSpaceF(orig_point);
  return gfx::RectF(transformed_point.x(), transformed_point.y(),
                    bounding_box.width(), bounding_box.height());
}

net::IsolationInfo ContentAutofillDriver::IsolationInfo() {
  return render_frame_host_->GetIsolationInfoForSubresources();
}

base::flat_map<FieldGlobalId, ServerFieldType>
ContentAutofillDriver::FillOrPreviewForm(
    int query_id,
    mojom::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  return GetAutofillRouter().FillOrPreviewForm(
      this, query_id, action, data, triggered_origin, field_type_map);
}

void ContentAutofillDriver::FillOrPreviewFormImpl(
    int query_id,
    mojom::RendererFormDataAction action,
    const FormData& data) {
  if (!RendererIsAvailable())
    return;

  GetAutofillAgent()->FillOrPreviewForm(query_id, data, action);
}

void ContentAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
  AutofillManager* manager = browser_autofill_manager_
                                 ? browser_autofill_manager_.get()
                                 : autofill_manager_.get();
  DCHECK(manager);
  manager->PropagateAutofillPredictions(render_frame_host_, forms);
}

void ContentAutofillDriver::HandleParsedForms(
    const std::vector<const FormData*>& forms) {
  // No op.
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRendererImpl(
    const std::vector<FormDataPredictions>& type_predictions) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->FieldTypePredictionsAvailable(type_predictions);
}

void ContentAutofillDriver::SendFieldsEligibleForManualFillingToRendererImpl(
    const std::vector<FieldRendererId>& fields) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->SetFieldsEligibleForManualFilling(fields);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestionImpl(
    const FieldRendererId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->AcceptDataListSuggestion(field, value);
}

void ContentAutofillDriver::RendererShouldClearFilledSectionImpl() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearSection();
}

void ContentAutofillDriver::RendererShouldClearPreviewedFormImpl() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentAutofillDriver::RendererShouldFillFieldWithValueImpl(
    const FieldRendererId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->FillFieldWithValue(field, value);
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValueImpl(
    const FieldRendererId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->PreviewFieldWithValue(field, value);
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailabilityImpl(
    const FieldRendererId& field,
    const mojom::AutofillState state) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->SetSuggestionAvailability(field, state);
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  // TODO(crbug.com/1185232) Send the FormDataPredictions object only if the
  // debugging flag is enabled.
  GetAutofillRouter().SendAutofillTypePredictionsToRenderer(this,
                                                            type_predictions);
}

void ContentAutofillDriver::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldGlobalId>& fields) {
  GetAutofillRouter().SendFieldsEligibleForManualFillingToRenderer(this,
                                                                   fields);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {
  GetAutofillRouter().RendererShouldAcceptDataListSuggestion(this, field,
                                                             value);
}

void ContentAutofillDriver::RendererShouldClearFilledSection() {
  GetAutofillRouter().RendererShouldClearFilledSection(this);
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  GetAutofillRouter().RendererShouldClearPreviewedForm(this);
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {
  GetAutofillRouter().RendererShouldFillFieldWithValue(this, field, value);
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {
  GetAutofillRouter().RendererShouldPreviewFieldWithValue(this, field, value);
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    const mojom::AutofillState state) {
  GetAutofillRouter().RendererShouldSetSuggestionAvailability(this, field,
                                                              state);
}

void ContentAutofillDriver::FormsSeenImpl(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  autofill_manager_->OnFormsSeen(updated_forms, removed_forms);
}

void ContentAutofillDriver::SetFormToBeProbablySubmittedImpl(
    const absl::optional<FormData>& form) {
  potentially_submitted_form_ = form;
}

void ContentAutofillDriver::FormSubmittedImpl(const FormData& form,
                                              bool known_success,
                                              mojom::SubmissionSource source) {
  // Omit duplicate form submissions. It may be reasonable to take |source|
  // into account here as well.
  // TODO(crbug/1117451): Clean up experiment code.
  if (base::FeatureList::IsEnabled(
          features::kAutofillProbableFormSubmissionInBrowser) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAllowDuplicateFormSubmissions) &&
      !submitted_forms_.insert(form.global_id()).second) {
    return;
  }

  autofill_manager_->OnFormSubmitted(form, known_success, source);
}

void ContentAutofillDriver::TextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    base::TimeTicks timestamp) {
  autofill_manager_->OnTextFieldDidChange(form, field, bounding_box, timestamp);
}

void ContentAutofillDriver::TextFieldDidScrollImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  autofill_manager_->OnTextFieldDidScroll(form, field, bounding_box);
}

void ContentAutofillDriver::SelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  autofill_manager_->OnSelectControlDidChange(form, field, bounding_box);
}

void ContentAutofillDriver::AskForValuesToFillImpl(
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  autofill_manager_->OnAskForValuesToFill(id, form, field, bounding_box,
                                          autoselect_first_suggestion);
}

void ContentAutofillDriver::HidePopupImpl() {
  DCHECK(!IsPrerendering()) << "We should never affect UI while prerendering";
  autofill_manager_->OnHidePopup();
}

void ContentAutofillDriver::FocusNoLongerOnFormImpl(bool had_interacted_form) {
  autofill_manager_->OnFocusNoLongerOnForm(had_interacted_form);
}

void ContentAutofillDriver::FocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  autofill_manager_->OnFocusOnFormField(form, field, bounding_box);
}

void ContentAutofillDriver::DidFillAutofillFormDataImpl(
    const FormData& form,
    base::TimeTicks timestamp) {
  autofill_manager_->OnDidFillAutofillFormData(form, timestamp);
}

void ContentAutofillDriver::DidPreviewAutofillFormDataImpl() {
  autofill_manager_->OnDidPreviewAutofillFormData();
}

void ContentAutofillDriver::DidEndTextFieldEditingImpl() {
  autofill_manager_->OnDidEndTextFieldEditing();
}

void ContentAutofillDriver::SelectFieldOptionsDidChangeImpl(
    const FormData& form) {
  autofill_manager_->SelectFieldOptionsDidChange(form);
}

void ContentAutofillDriver::FillFormForAssistantImpl(
    const AutofillableData& fill_data,
    const FormData& form,
    const FormFieldData& field) {
  DCHECK(browser_autofill_manager_);
  if (fill_data.is_profile()) {
    browser_autofill_manager_->FillProfileForm(fill_data.profile(), form,
                                               field);
  } else if (fill_data.is_credit_card()) {
    browser_autofill_manager_->FillCreditCardForm(
        /*query_id=*/kNoQueryId, form, field, fill_data.credit_card(),
        fill_data.cvc());
  } else {
    NOTREACHED();
  }
}

void ContentAutofillDriver::ProbablyFormSubmitted() {
  if (potentially_submitted_form_.has_value()) {
    GetAutofillRouter().FormSubmitted(
        this, potentially_submitted_form_.value(), false,
        mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
}

void ContentAutofillDriver::SetFormToBeProbablySubmitted(
    const absl::optional<FormData>& form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().SetFormToBeProbablySubmitted(
      this, form ? absl::make_optional<FormData>(
                       GetFormWithFrameAndFormMetaData(*form))
                 : absl::nullopt);
}

void ContentAutofillDriver::FormsSeen(
    const std::vector<FormData>& raw_updated_forms,
    const std::vector<FormRendererId>& raw_removed_forms) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  std::vector<FormData> updated_forms = raw_updated_forms;
  for (FormData& form : updated_forms)
    SetFrameAndFormMetaData(form, nullptr);

  LocalFrameToken frame_token(render_frame_host_->GetFrameToken().value());
  std::vector<FormGlobalId> removed_forms;
  removed_forms.reserve(raw_removed_forms.size());
  for (FormRendererId form_id : raw_removed_forms)
    removed_forms.push_back({frame_token, form_id});

  GetAutofillRouter().FormsSeen(this, updated_forms, removed_forms);
}

void ContentAutofillDriver::FormSubmitted(const FormData& raw_form,
                                          bool known_success,
                                          mojom::SubmissionSource source) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().FormSubmitted(
      this, GetFormWithFrameAndFormMetaData(raw_form), known_success, source);
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box,
                                               base::TimeTicks timestamp) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().TextFieldDidChange(
      this, form, field,
      TransformBoundingBoxToViewportCoordinates(bounding_box), timestamp);
}

void ContentAutofillDriver::TextFieldDidScroll(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().TextFieldDidScroll(
      this, form, field,
      TransformBoundingBoxToViewportCoordinates(bounding_box));
}

void ContentAutofillDriver::SelectControlDidChange(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().SelectControlDidChange(
      this, form, field,
      TransformBoundingBoxToViewportCoordinates(bounding_box));
}

void ContentAutofillDriver::AskForValuesToFill(
    int32_t id,
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().AskForValuesToFill(
      this, id, form, field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      autoselect_first_suggestion);
}

void ContentAutofillDriver::HidePopup() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().HidePopup(this);
}

void ContentAutofillDriver::FocusNoLongerOnForm(bool had_interacted_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().FocusNoLongerOnForm(this, had_interacted_form);
}

void ContentAutofillDriver::FocusOnFormField(const FormData& raw_form,
                                             const FormFieldData& raw_field,
                                             const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().FocusOnFormField(
      this, form, field,
      TransformBoundingBoxToViewportCoordinates(bounding_box));
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& raw_form,
                                                    base::TimeTicks timestamp) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().DidFillAutofillFormData(
      this, GetFormWithFrameAndFormMetaData(raw_form), timestamp);
}

void ContentAutofillDriver::DidPreviewAutofillFormData() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().DidPreviewAutofillFormData(this);
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().DidEndTextFieldEditing(this);
}

void ContentAutofillDriver::SelectFieldOptionsDidChange(
    const FormData& raw_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host_))
    return;
  GetAutofillRouter().SelectFieldOptionsDidChange(
      this, GetFormWithFrameAndFormMetaData(raw_form));
}

void ContentAutofillDriver::FillFormForAssistant(
    const AutofillableData& fill_data,
    const FormData& raw_form,
    const FormFieldData& raw_field) {
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  GetAutofillRouter().FillFormForAssistant(this, fill_data, form, field);
}

void ContentAutofillDriver::DidNavigateFrame(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    // On page refresh, reset the rate limiter for fetching authentication
    // details for credit card unmasking.
    if (browser_autofill_manager_) {
      browser_autofill_manager_->credit_card_access_manager()
          ->SignalCanFetchUnmaskDetails();
    }
    return;
  }

  ShowOfferNotificationIfApplicable(navigation_handle);

  // When IsServedFromBackForwardCache or IsPrerendererdPageActivation, the form
  // data is not parsed again. So, we should keep and use the autofill manager's
  // form_structures from BFCache or prerendering page for form submit.
  if (navigation_handle->IsServedFromBackForwardCache() ||
      navigation_handle->IsPrerenderedPageActivation()) {
    return;
  }

  // The driver's RenderFrameHost may be used for the page we're navigating to.
  // Therefore, we need to forget all forms of the page we're navigating from.
  submitted_forms_.clear();
  if (autofill_router_)  // Can be nullptr only in tests.
    autofill_router_->UnregisterDriver(this);
  autofill_manager_->Reset();
}

void ContentAutofillDriver::SetBrowserAutofillManager(
    std::unique_ptr<BrowserAutofillManager> manager) {
  autofill_manager_ = std::move(manager);
  browser_autofill_manager_ =
      static_cast<BrowserAutofillManager*>(autofill_manager_.get());
}

ContentAutofillDriver::ContentAutofillDriver(content::RenderFrameHost* rfh)
    : render_frame_host_(rfh),
      browser_autofill_manager_(nullptr),
      log_manager_(nullptr) {}

const mojo::AssociatedRemote<mojom::AutofillAgent>&
ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_agent_);
  }

  return autofill_agent_;
}

void ContentAutofillDriver::SetKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  GetAutofillRouter().SetKeyPressHandler(this, handler);
}

void ContentAutofillDriver::UnsetKeyPressHandler() {
  GetAutofillRouter().UnsetKeyPressHandler(this);
}

void ContentAutofillDriver::SetKeyPressHandlerImpl(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  UnsetKeyPressHandlerImpl();
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->AddKeyPressEventCallback(handler);
  key_press_handler_ = handler;
}

void ContentAutofillDriver::UnsetKeyPressHandlerImpl() {
  if (key_press_handler_.is_null())
    return;
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->RemoveKeyPressEventCallback(key_press_handler_);
  key_press_handler_.Reset();
}

void ContentAutofillDriver::SetFrameAndFormMetaData(
    FormData& form,
    FormFieldData* optional_field) const {
  form.host_frame =
      LocalFrameToken(render_frame_host_->GetFrameToken().value());

  // GetLastCommittedURL doesn't include URL updates due to document.open() and
  // so it might be about:blank or about:srcdoc. In this case fallback to
  // GetLastCommittedOrigin. See http://crbug.com/1209270 for more details.
  GURL url = render_frame_host_->GetLastCommittedURL();
  if (url.SchemeIs(url::kAboutScheme))
    url = render_frame_host_->GetLastCommittedOrigin().GetURL();
  form.url = StripAuthAndParams(url);

  if (auto* main_rfh = render_frame_host_->GetMainFrame())
    form.main_frame_origin = main_rfh->GetLastCommittedOrigin();
  else
    form.main_frame_origin = url::Origin();

  // The form signature must not be calculated before setting FormData::url.
  FormSignature form_signature = CalculateFormSignature(form);

  auto SetFieldMetaData = [&](FormFieldData& field) {
    field.host_frame = form.host_frame;
    field.host_form_id = form.unique_renderer_id;
    field.origin = render_frame_host_->GetLastCommittedOrigin();
    field.host_form_signature = form_signature;
  };

  for (FormFieldData& field : form.fields)
    SetFieldMetaData(field);
  if (optional_field)
    SetFieldMetaData(*optional_field);
}

FormData ContentAutofillDriver::GetFormWithFrameAndFormMetaData(
    FormData form) const {
  SetFrameAndFormMetaData(form, nullptr);
  return form;
}

bool ContentAutofillDriver::DocumentUsedWebOTP() const {
  return render_frame_host_->DocumentUsedWebOTP();
}

void ContentAutofillDriver::MaybeReportAutofillWebOTPMetrics() {
  // In tests, the browser_autofill_manager_ may be unset or destroyed before
  // |this|.
  if (!browser_autofill_manager_)
    return;
  // It's possible that a frame without any form uses WebOTP. e.g. a server may
  // send the verification code to a phone number that was collected beforehand
  // and uses the WebOTP API for authentication purpose without user manually
  // entering the code.
  if (!browser_autofill_manager_->has_parsed_forms() && !DocumentUsedWebOTP())
    return;

  ReportAutofillWebOTPMetrics(DocumentUsedWebOTP());
}

void ContentAutofillDriver::ReportAutofillWebOTPMetrics(
    bool document_used_webotp) {
  if (browser_autofill_manager_->has_observed_phone_number_field())
    phone_collection_metric_state_ |= phone_collection_metric::kPhoneCollected;
  if (browser_autofill_manager_->has_observed_one_time_code_field())
    phone_collection_metric_state_ |= phone_collection_metric::kOTCUsed;
  if (document_used_webotp)
    phone_collection_metric_state_ |= phone_collection_metric::kWebOTPUsed;

  ukm::UkmRecorder* recorder =
      browser_autofill_manager_->client()->GetUkmRecorder();
  ukm::SourceId source_id =
      browser_autofill_manager_->client()->GetUkmSourceId();
  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      recorder, source_id, phone_collection_metric_state_);

  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      static_cast<PhoneCollectionMetricState>(phone_collection_metric_state_));
}

void ContentAutofillDriver::ShowOfferNotificationIfApplicable(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // Android webview does not have |browser_autofill_manager_|, so flow is not
  // enabled in Android Webview.
  if (!browser_autofill_manager_)
    return;

  AutofillOfferManager* offer_manager =
      browser_autofill_manager_->offer_manager();
  // This happens in the Incognito mode.
  if (!offer_manager)
    return;

  GURL url = browser_autofill_manager_->client()->GetLastCommittedURL();
  if (!offer_manager->IsUrlEligible(url))
    return;

  // Try to show offer notification when the last committed URL has the domain
  // that an offer is applicable for.
  // TODO(crbug.com/1203811): GetOfferForUrl needs to know whether to give
  //   precedence to card-linked offers or promo code offers. Eventually, promo
  //   code offers should take precedence if a bubble is shown. Currently, if a
  //   url has both types of offers and the promo code offer is selected, no
  //   bubble will end up being shown (due to not yet being implemented).
  AutofillOfferData* offer = offer_manager->GetOfferForUrl(url);

  if (!offer || offer->merchant_origins.empty() ||
      (offer->IsCardLinkedOffer() && offer->eligible_instrument_id.empty()) ||
      (offer->IsPromoCodeOffer() && offer->promo_code.empty())) {
    return;
  }

  browser_autofill_manager_->client()->ShowOfferNotificationIfApplicable(offer);
}

ContentAutofillRouter& ContentAutofillDriver::GetAutofillRouter() {
  DCHECK(content::RenderFrameHost::LifecycleState::kPrerendering !=
         render_frame_host_->GetLifecycleState());
  return *autofill_router_;
}

}  // namespace autofill
