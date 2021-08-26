// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
void AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager) {
  // By the time this method is called, no web_frame is available. This method
  // only prepares the factory and the AutofillDriverIOS will be created in the
  // first call to FromWebStateAndWebFrame.
  AutofillDriverIOSWebFrameFactory::CreateForWebStateAndDelegate(
      web_state, client, bridge, app_locale, enable_download_manager);
}

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
    return AutofillDriverIOSWebFrameFactory::FromWebState(web_state)
        ->AutofillDriverIOSFromWebFrame(web_frame)
        ->driver();
}

AutofillDriverIOS::AutofillDriverIOS(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager)
    : web_state_(web_state),
      bridge_(bridge),
      browser_autofill_manager_(this,
                                client,
                                app_locale,
                                enable_download_manager) {
  web_frame_id_ = web::GetWebFrameId(web_frame);
}

AutofillDriverIOS::~AutofillDriverIOS() {}

bool AutofillDriverIOS::IsIncognito() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

bool AutofillDriverIOS::IsInMainFrame() const {
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  return web_frame ? web_frame->IsMainFrame() : true;
}

bool AutofillDriverIOS::IsPrerendering() const {
  return false;
}

bool AutofillDriverIOS::CanShowAutofillUi() const {
  return true;
}

ui::AXTreeID AutofillDriverIOS::GetAxTreeId() const {
  NOTIMPLEMENTED() << "See https://crbug.com/985933";
  return ui::AXTreeIDUnknown();
}

scoped_refptr<network::SharedURLLoaderFactory>
AutofillDriverIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state_->GetBrowserState()->GetURLLoaderFactory());
}

bool AutofillDriverIOS::RendererIsAvailable() {
  return true;
}

void AutofillDriverIOS::FillOrPreviewForm(
    int query_id,
    mojom::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame) {
    return;
  }
  [bridge_ fillFormData:data inFrame:web_frame];
}

void AutofillDriverIOS::PropagateAutofillPredictions(
    const std::vector<autofill::FormStructure*>& forms) {
  browser_autofill_manager_.client()->PropagateAutofillPredictions(nullptr,
                                                                   forms);
}

void AutofillDriverIOS::HandleParsedForms(
    const std::vector<const FormData*>& forms) {
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& map =
      browser_autofill_manager_.form_structures();
  std::vector<FormStructure*> form_structures;
  form_structures.reserve(forms.size());
  for (const FormData* form : forms) {
    auto it = map.find(form->global_id());
    if (it != map.end())
      form_structures.push_back(it->second.get());
  }

  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame) {
    return;
  }
  [bridge_ handleParsedForms:form_structures inFrame:web_frame];
}

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame) {
    return;
  }
  [bridge_ fillFormDataPredictions:FormStructure::GetFieldTypePredictions(forms)
                           inFrame:web_frame];
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldGlobalId>& fields) {}

void AutofillDriverIOS::RendererShouldClearFilledSection() {}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {
}

void AutofillDriverIOS::RendererShouldFillFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::RendererShouldPreviewFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    const mojom::AutofillState state) {}

void AutofillDriverIOS::PopupHidden() {
}

net::IsolationInfo AutofillDriverIOS::IsolationInfo() {
  std::string main_web_frame_id = web::GetMainWebFrameId(web_state_);
  web::WebFrame* main_web_frame =
      web::GetWebFrameWithId(web_state_, main_web_frame_id);
  if (!main_web_frame)
    return net::IsolationInfo();

  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame)
    return net::IsolationInfo();

  return net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      url::Origin::Create(main_web_frame->GetSecurityOrigin()),
      url::Origin::Create(web_frame->GetSecurityOrigin()),
      net::SiteForCookies());
}

}  // namespace autofill
