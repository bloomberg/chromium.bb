// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

FrameResourceFetcherProperties::FrameResourceFetcherProperties(
    FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document) {}

void FrameResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  ResourceFetcherProperties::Trace(visitor);
}

bool FrameResourceFetcherProperties::IsMainFrame() const {
  return frame_or_imported_document_->GetFrame().IsMainFrame();
}

mojom::ControllerServiceWorkerMode
FrameResourceFetcherProperties::GetControllerServiceWorkerMode() const {
  auto* service_worker_network_provider =
      frame_or_imported_document_->GetMasterDocumentLoader()
          .GetServiceWorkerNetworkProvider();
  if (!service_worker_network_provider)
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return service_worker_network_provider->IsControlledByServiceWorker();
}

int64_t FrameResourceFetcherProperties::ServiceWorkerId() const {
  DCHECK_NE(GetControllerServiceWorkerMode(),
            blink::mojom::ControllerServiceWorkerMode::kNoController);
  auto* service_worker_network_provider =
      frame_or_imported_document_->GetMasterDocumentLoader()
          .GetServiceWorkerNetworkProvider();
  DCHECK(service_worker_network_provider);
  return service_worker_network_provider->ControllerServiceWorkerID();
}

bool FrameResourceFetcherProperties::IsPaused() const {
  return frame_or_imported_document_->GetFrame().GetPage()->Paused();
}

bool FrameResourceFetcherProperties::IsLoadComplete() const {
  Document* document = frame_or_imported_document_->GetDocument();
  return document && document->LoadEventFinished();
}

bool FrameResourceFetcherProperties::ShouldBlockLoadingMainResource() const {
  DocumentLoader* document_loader =
      frame_or_imported_document_->GetDocumentLoader();
  if (!document_loader)
    return false;

  FrameLoader& loader = frame_or_imported_document_->GetFrame().Loader();
  return document_loader != loader.GetProvisionalDocumentLoader();
}

bool FrameResourceFetcherProperties::ShouldBlockLoadingSubResource() const {
  DocumentLoader* document_loader =
      frame_or_imported_document_->GetDocumentLoader();
  if (!document_loader)
    return false;

  FrameLoader& loader = frame_or_imported_document_->GetFrame().Loader();
  return document_loader != loader.GetDocumentLoader();
}

}  // namespace blink
