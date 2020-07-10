// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/network_isolation_key.h"

#if !defined(OS_IOS)
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"
#endif

namespace network {
class SharedURLLoaderFactory;
}

namespace gfx {
class RectF;
}

namespace ui {
class AXTreeID;
}

namespace autofill {

class FormStructure;

// Interface that allows Autofill core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class AutofillDriver {
 public:
   // The possible actions that the renderer can take on receiving form data.
  enum RendererFormDataAction {
    // The renderer should fill the form data.
    FORM_DATA_ACTION_FILL,
    // The renderer should preview the form data.
    FORM_DATA_ACTION_PREVIEW
  };

  virtual ~AutofillDriver() {}

  // Returns whether the user is currently operating in an incognito context.
  virtual bool IsIncognito() const = 0;

  // Returns whether AutofillDriver instance is associated to the main frame.
  virtual bool IsInMainFrame() const = 0;

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
  // Binds the mojom request in order to facilitate WebAuthn flows.
  virtual void ConnectToAuthenticator(
      mojo::PendingReceiver<blink::mojom::InternalAuthenticator> receiver) = 0;
#endif

  // Forwards |data| to the renderer. |query_id| is the id of the renderer's
  // original request for the data. |action| is the action the renderer should
  // perform with the |data|. This method is a no-op if the renderer is not
  // currently available.
  virtual void SendFormDataToRenderer(int query_id,
                                      RendererFormDataAction action,
                                      const FormData& data) = 0;

  // Pass the form structures to the password manager to choose correct username
  // and to the password generation manager to detect account creation forms.
  virtual void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) = 0;

  // Forwards parsed |forms| to the embedder.
  virtual void HandleParsedForms(const std::vector<FormStructure*>& forms) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) = 0;

  // Tells the renderer to clear the current section of the autofilled values.
  virtual void RendererShouldClearFilledSection() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  // Tells the renderer to set the node text.
  virtual void RendererShouldFillFieldWithValue(
      const base::string16& value) = 0;

  // Tells the renderer to preview the node with suggested text.
  virtual void RendererShouldPreviewFieldWithValue(
      const base::string16& value) = 0;

  // Tells the renderer to set the currently focused node's corresponding
  // accessibility node's autofill state to |state|.
  virtual void RendererShouldSetSuggestionAvailability(
      const mojom::AutofillState state) = 0;

  // Informs the renderer that the popup has been hidden.
  virtual void PopupHidden() = 0;

  // Transform bounding box coordinates to real viewport coordinates. In
  // the case of a page spanning multiple renderer processes, subframe
  // renderers cannot do this transformation themselves.
  virtual gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) = 0;

  virtual net::NetworkIsolationKey NetworkIsolationKey() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
