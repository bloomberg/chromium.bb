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
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

FrameResourceFetcherProperties::FrameResourceFetcherProperties(
    FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document),
      fetch_client_settings_object_(
          CreateFetchClientSettingsObject(frame_or_imported_document)) {}

void FrameResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

void FrameResourceFetcherProperties::UpdateDocument(Document& document) {
  frame_or_imported_document_->UpdateDocument(document);
  fetch_client_settings_object_ =
      CreateFetchClientSettingsObject(*frame_or_imported_document_);
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

const FetchClientSettingsObject&
FrameResourceFetcherProperties::CreateFetchClientSettingsObject(
    const FrameOrImportedDocument& frame_or_imported_document) {
  if (frame_or_imported_document.GetDocument()) {
    // HTML imports case
    return *MakeGarbageCollected<FetchClientSettingsObjectImpl>(
        *frame_or_imported_document.GetDocument());
  }

  // This FetchClientSettingsObject can be used only for navigation, as
  // at the creation of the corresponding Document a new
  // FetchClientSettingsObject is set.
  // Also, currently all the members except for SecurityOrigin are not
  // used in FrameFetchContext, and therefore we set some safe default
  // values here.
  // Once PlzNavigate removes ResourceFetcher usage in navigations, we
  // might be able to remove this FetchClientSettingsObject at all.
  return *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
      KURL(),

      // SecurityOrigin. This is actually used via
      // FetchContext::GetSecurityOrigin().
      // TODO(hiroshige): Assign non-nullptr SecurityOrigin.
      nullptr,

      // Currently this is not used, and referrer for navigation request
      // is set based on previous Document's referrer policy, for example
      // in FrameLoader::SetReferrerForFrameRequest().
      // If we want to set referrer based on FetchClientSettingsObject,
      // it should refer to the FetchClientSettingsObject of the previous
      // Document, probably not this one.
      network::mojom::ReferrerPolicy::kDefault, String(),

      // MixedContentChecker::ShouldBlockFetch() doesn't check mixed
      // contents if frame type is not kNone, which is always true in
      // RawResource::FetchMainResource().
      // Therefore HttpsState here isn't (and isn't expected to be)
      // used and thus it's safe to pass a safer default value.
      HttpsState::kModern,

      // This is only for workers and this value is not (and isn't
      // expected to be) used.
      AllowedByNosniff::MimeTypeCheck::kStrict);
}

}  // namespace blink
