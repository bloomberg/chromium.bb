// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/origin.h"

#if !defined(OS_IOS)
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace autofill {

// This class is only for easier writing of tests.
#if defined(OS_IOS)
class TestAutofillDriver : public AutofillDriver {
#else
class TestAutofillDriver : public ContentAutofillDriver {
#endif
 public:
  TestAutofillDriver();
  TestAutofillDriver(const TestAutofillDriver&) = delete;
  TestAutofillDriver& operator=(const TestAutofillDriver&) = delete;
  ~TestAutofillDriver() override;

  // AutofillDriver implementation overrides.
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  bool IsPrerendering() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
#if !defined(OS_IOS)
  webauthn::InternalAuthenticator* GetOrCreateCreditCardInternalAuthenticator()
      override;
#endif
  // The return value contains the members (field, type) of `field_type_map` for
  // which `field_type_filter_.Run(triggered_origin, field, type)` is true.
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
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
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
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;

  // Methods unique to TestAutofillDriver that tests can use to specialize
  // functionality.

  void SetIsIncognito(bool is_incognito);
  void SetIsInMainFrame(bool is_in_main_frame);
  void SetIsolationInfo(const net::IsolationInfo& isolation_info);

  // The filter that determines the return value of FillOrPreviewForm().
  void SetFieldTypeMapFilter(
      base::RepeatingCallback<
          bool(const url::Origin&, FieldGlobalId, ServerFieldType)> callback);

  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
#if !defined(OS_IOS)
  void SetAuthenticator(webauthn::InternalAuthenticator* authenticator_);
#endif

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  bool is_incognito_ = false;
  bool is_in_main_frame_ = false;
  net::IsolationInfo isolation_info_;
  base::RepeatingCallback<
      bool(const url::Origin&, FieldGlobalId, ServerFieldType)>
      field_type_map_filter_;

#if !defined(OS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator> test_authenticator_;
#endif
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
