// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/key_press_handler_manager.h"
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace autofill {

class AutofillClient;
class AutofillProvider;
class LogManager;

// Class that drives autofill flow in the browser process based on
// communication from the renderer and from the external world. There is one
// instance per RenderFrameHost.
class ContentAutofillDriver : public AutofillDriver,
                              public mojom::AutofillDriver,
                              public KeyPressHandlerManager::Delegate {
 public:
  ContentAutofillDriver(
      content::RenderFrameHost* render_frame_host,
      AutofillClient* client,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager,
      AutofillProvider* provider);
  ~ContentAutofillDriver() override;

  // Gets the driver for |render_frame_host|.
  static ContentAutofillDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver);

  // AutofillDriver:
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
  InternalAuthenticator* GetOrCreateCreditCardInternalAuthenticator() override;
  void SendFormDataToRenderer(int query_id,
                              RendererFormDataAction action,
                              const FormData& data) override;
  void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) override;
  void HandleParsedForms(const std::vector<FormStructure*>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldFillFieldWithValue(const base::string16& value) override;
  void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;
  void RendererShouldSetSuggestionAvailability(
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;
  net::IsolationInfo IsolationInfo() override;

  // mojom::AutofillDriver:
  void FormsSeen(const std::vector<FormData>& forms,
                 base::TimeTicks timestamp) override;
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource source) override;
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override;
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void QueryFormFieldAutofill(int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box,
                              bool autoselect_first_suggestion) override;
  void HidePopup() override;
  void FocusNoLongerOnForm() override;
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void DidPreviewAutofillFormData() override;
  void DidEndTextFieldEditing() override;
  void SetDataList(const std::vector<base::string16>& values,
                   const std::vector<base::string16>& labels) override;
  void SelectFieldOptionsDidChange(const FormData& form) override;

  // Called when the main frame has navigated. Explicitely will not trigger for
  // subframe navigations. See navigation_handle.h for details.
  void DidNavigateMainFrame(content::NavigationHandle* navigation_handle);

  AutofillManager* autofill_manager() { return autofill_manager_; }
  AutofillHandler* autofill_handler() { return autofill_handler_.get(); }
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  const mojo::AssociatedRemote<mojom::AutofillAgent>& GetAutofillAgent();

  // Methods forwarded to key_press_handler_manager_.
  void RegisterKeyPressHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler);
  void RemoveKeyPressHandler();

  void SetAutofillProviderForTesting(AutofillProvider* provider);

 protected:
  // Sets the manager to |manager| and sets |manager|'s external delegate
  // to |autofill_external_delegate_|. Takes ownership of |manager|.
  void SetAutofillManager(std::unique_ptr<AutofillManager> manager);

 private:
  // KeyPressHandlerManager::Delegate:
  void AddHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler) override;
  void RemoveHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler) override;

  void SetAutofillProvider(AutofillProvider* provider);

  // Weak ref to the RenderFrameHost the driver is associated with. Should
  // always be non-NULL and valid for lifetime of |this|.
  content::RenderFrameHost* const render_frame_host_;

  // AutofillHandler instance via which this object drives the shared Autofill
  // code.
  std::unique_ptr<AutofillHandler> autofill_handler_;

  // The pointer to autofill_handler_ if it is AutofillManager instance.
  // TODO: unify autofill_handler_ and autofill_manager_ to a single pointer to
  // a common root.
  AutofillManager* autofill_manager_;

  // Pointer to an implementation of InternalAuthenticator.
  std::unique_ptr<InternalAuthenticator> authenticator_impl_;

  // AutofillExternalDelegate instance that this object instantiates in the
  // case where the Autofill native UI is enabled.
  std::unique_ptr<AutofillExternalDelegate> autofill_external_delegate_;

  KeyPressHandlerManager key_press_handler_manager_;

  LogManager* const log_manager_;

  mojo::AssociatedReceiver<mojom::AutofillDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAgent> autofill_agent_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
