// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_CONTENT_AUTOFILL_ASSISTANT_DRIVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_CONTENT_AUTOFILL_ASSISTANT_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "components/autofill_assistant/content/common/autofill_assistant_driver.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace autofill_assistant {

// ContentAutofillAssistantDriver operates in the browser process based on
// communication from the renderer. There is one instance per RenderFrameHost.
class ContentAutofillAssistantDriver
    : public mojom::AutofillAssistantDriver,
      public content::DocumentUserData<ContentAutofillAssistantDriver> {
 public:
  ~ContentAutofillAssistantDriver() override;

  ContentAutofillAssistantDriver(const ContentAutofillAssistantDriver&) =
      delete;
  ContentAutofillAssistantDriver& operator=(
      const ContentAutofillAssistantDriver&) = delete;

  static void BindDriver(mojo::PendingAssociatedReceiver<
                             mojom::AutofillAssistantDriver> pending_receiver,
                         content::RenderFrameHost* render_frame_host);

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>
          pending_receiver);

  const mojo::AssociatedRemote<mojom::AutofillAssistantAgent>&
  GetAutofillAssistantAgent();

  void SetAnnotateDomModelService(
      AnnotateDomModelService* annotate_dom_model_service);

  // autofill_assistant::mojom::AutofillAssistantDriver:
  void GetAnnotateDomModel(GetAnnotateDomModelCallback callback) override;

 private:
  explicit ContentAutofillAssistantDriver(
      content::RenderFrameHost* render_frame_host);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  void OnModelAvailabilityChanged(GetAnnotateDomModelCallback callback,
                                  bool is_available);

  raw_ptr<AnnotateDomModelService> annotate_dom_model_service_ = nullptr;

  mojo::AssociatedReceiver<mojom::AutofillAssistantDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAssistantAgent>
      autofill_assistant_agent_;

  base::WeakPtrFactory<ContentAutofillAssistantDriver> weak_pointer_factory_{
      this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_CONTENT_AUTOFILL_ASSISTANT_DRIVER_H_
