// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FEDERATED_LEARNING_FLOC_SERVICE_IMPL_H_
#define CONTENT_BROWSER_FEDERATED_LEARNING_FLOC_SERVICE_IMPL_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace content {

class RenderFrameHost;

class FlocServiceImpl final
    : public DocumentService<blink::mojom::FlocService> {
 public:
  FlocServiceImpl(RenderFrameHost* render_frame_host,
                  mojo::PendingReceiver<blink::mojom::FlocService> receiver);

  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::FlocService> receiver);

  // blink::mojom::FlocService.
  void GetInterestCohort(GetInterestCohortCallback callback) override;

 private:
  // |this| can only be destroyed by DocumentService.
  ~FlocServiceImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FEDERATED_LEARNING_FLOC_SERVICE_IMPL_H_
