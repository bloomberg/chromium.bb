// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "url/origin.h"

namespace web {
class WebFrame;
class WebState;
}

@protocol AutofillDriverIOSBridge;

namespace autofill {

// Class that drives autofill flow on iOS. There is one instance per
// WebContents.
class AutofillDriverIOS : public AutofillDriver {
 public:
  ~AutofillDriverIOS() override;

  static void PrepareForWebStateWebFrameAndDelegate(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      BrowserAutofillManager::AutofillDownloadManagerState
          enable_download_manager);

  static AutofillDriverIOS* FromWebStateAndWebFrame(web::WebState* web_state,
                                                    web::WebFrame* web_frame);

  // AutofillDriver:
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  bool IsPrerendering() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
  base::flat_map<FieldGlobalId, ServerFieldType> FillOrPreviewForm(
      int query_id,
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) override;
  void HandleParsedForms(const std::vector<const FormData*>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;

  BrowserAutofillManager* autofill_manager() {
    return &browser_autofill_manager_;
  }

  void RendererShouldFillFieldWithValue(const FieldGlobalId& field,
                                        const std::u16string& value) override;
  void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;

  bool is_processed() const { return processed_; }
  void set_processed(bool processed) { processed_ = processed; }

 protected:
  AutofillDriverIOS(web::WebState* web_state,
                    web::WebFrame* web_frame,
                    AutofillClient* client,
                    id<AutofillDriverIOSBridge> bridge,
                    const std::string& app_locale,
                    BrowserAutofillManager::AutofillDownloadManagerState
                        enable_download_manager);

 private:
  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // The id of the WebFrame with which this object is associated.
  // "" if frame messaging is disabled.
  std::string web_frame_id_;

  // AutofillDriverIOSBridge instance that is passed in.
  __unsafe_unretained id<AutofillDriverIOSBridge> bridge_;

  // Whether the initial processing has been done (JavaScript observers have
  // been enabled and the forms have been extracted).
  bool processed_ = false;

  // BrowserAutofillManager instance via which this object drives the shared
  // Autofill code.
  BrowserAutofillManager browser_autofill_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
