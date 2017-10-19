// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/storage/DOMWindowStorage.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "modules/storage/Storage.h"
#include "modules/storage/StorageNamespace.h"
#include "modules/storage/StorageNamespaceController.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

DOMWindowStorage::DOMWindowStorage(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void DOMWindowStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_storage_);
  visitor->Trace(local_storage_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char* DOMWindowStorage::SupplementName() {
  return "DOMWindowStorage";
}

// static
DOMWindowStorage& DOMWindowStorage::From(LocalDOMWindow& window) {
  DOMWindowStorage* supplement = static_cast<DOMWindowStorage*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement) {
    supplement = new DOMWindowStorage(window);
    ProvideTo(window, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Storage* DOMWindowStorage::sessionStorage(LocalDOMWindow& window,
                                          ExceptionState& exception_state) {
  return From(window).sessionStorage(exception_state);
}

// static
Storage* DOMWindowStorage::localStorage(LocalDOMWindow& window,
                                        ExceptionState& exception_state) {
  return From(window).localStorage(exception_state);
}

Storage* DOMWindowStorage::sessionStorage(
    ExceptionState& exception_state) const {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  Document* document = GetSupplementable()->GetFrame()->GetDocument();
  DCHECK(document);
  String access_denied_message = "Access is denied for this document.";
  if (!document->GetSecurityOrigin()->CanAccessLocalStorage()) {
    if (document->IsSandboxed(kSandboxOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }

  if (session_storage_) {
    if (!session_storage_->Area()->CanAccessStorage(document->GetFrame())) {
      exception_state.ThrowSecurityError(access_denied_message);
      return nullptr;
    }
    return session_storage_;
  }

  Page* page = document->GetPage();
  if (!page)
    return nullptr;

  StorageArea* storage_area =
      StorageNamespaceController::From(page)->SessionStorage()->GetStorageArea(
          document->GetSecurityOrigin());
  if (!storage_area->CanAccessStorage(document->GetFrame())) {
    exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }

  session_storage_ = Storage::Create(document->GetFrame(), storage_area);
  return session_storage_;
}

Storage* DOMWindowStorage::localStorage(ExceptionState& exception_state) const {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  Document* document = GetSupplementable()->GetFrame()->GetDocument();
  DCHECK(document);
  String access_denied_message = "Access is denied for this document.";
  if (!document->GetSecurityOrigin()->CanAccessLocalStorage()) {
    if (document->IsSandboxed(kSandboxOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }
  if (local_storage_) {
    if (!local_storage_->Area()->CanAccessStorage(document->GetFrame())) {
      exception_state.ThrowSecurityError(access_denied_message);
      return nullptr;
    }
    return local_storage_;
  }
  // FIXME: Seems this check should be much higher?
  Page* page = document->GetPage();
  if (!page || !page->GetSettings().GetLocalStorageEnabled())
    return nullptr;
  StorageArea* storage_area =
      StorageNamespace::LocalStorageArea(document->GetSecurityOrigin());
  if (!storage_area->CanAccessStorage(document->GetFrame())) {
    exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }
  local_storage_ = Storage::Create(document->GetFrame(), storage_area);
  return local_storage_;
}

}  // namespace blink
