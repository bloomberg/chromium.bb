// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/isolation_info.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

#if !defined(OS_IOS)
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace network {
class SharedURLLoaderFactory;
}

namespace autofill {

class FormStructure;

// Interface that allows Autofill core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class AutofillDriver {
 public:
  virtual ~AutofillDriver() {}

  // Returns whether the user is currently operating in an incognito context.
  virtual bool IsIncognito() const = 0;

  // Returns whether AutofillDriver instance is associated with a main frame.
  virtual bool IsInMainFrame() const = 0;

  // Returns whether the AutofillDriver instance is associated with a
  // prerendered frame.
  virtual bool IsPrerendering() const = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  // Returns the ax tree id associated with this driver.
  virtual ui::AXTreeID GetAxTreeId() const = 0;

  // Returns the URL loader factory associated with this driver.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns true iff the renderer is available for communication.
  virtual bool RendererIsAvailable() = 0;

#if !defined(OS_IOS)
  // Gets or creates a pointer to an implementation of InternalAuthenticator.
  virtual webauthn::InternalAuthenticator*
  GetOrCreateCreditCardInternalAuthenticator() = 0;
#endif

  // Forwards |data| to the renderer which shall preview or fill the values of
  // |data|'s fields into the relevant DOM elements.
  //
  // |query_id| is the id of the renderer's original request for the data.
  //
  // |action| is the action the renderer should perform with the |data|.
  //
  // |triggered_origin| is the origin of the field on which Autofill was
  // triggered, and |field_type_map| contains the type predictions of the fields
  // that may be previewed or filled; these two parameters can be taken into
  // account to decide which fields to preview or fill across frames.
  //
  // Returns a map from actually previewed or filled fields to their type. This
  // is a sub-map of |field_type_map|: it does not contain those fields from
  // |field_type_map| which shall not be filled due to the security policy for
  // cross-frame previewing and filling.
  //
  // This method is a no-op if the renderer is not currently available.
  virtual base::flat_map<FieldGlobalId, ServerFieldType> FillOrPreviewForm(
      int query_id,
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) = 0;

  // Pass the form structures to the password manager to choose correct username
  // and to the password generation manager to detect account creation forms.
  virtual void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) = 0;

  // Forwards parsed |forms| to the embedder.
  virtual void HandleParsedForms(const std::vector<const FormData*>& forms) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to clear the current section of the autofilled values.
  virtual void RendererShouldClearFilledSection() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  // Tells the renderer to set the node text.
  virtual void RendererShouldFillFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to preview the node with suggested text.
  virtual void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to set the currently focused node's corresponding
  // accessibility node's autofill state to |state|.
  virtual void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      const mojom::AutofillState state) = 0;

  // Informs the renderer that the popup has been hidden.
  virtual void PopupHidden() = 0;

  virtual net::IsolationInfo IsolationInfo() = 0;

  // Tells the renderer about the form fields that are eligible for triggering
  // manual filling on form interaction.
  virtual void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
