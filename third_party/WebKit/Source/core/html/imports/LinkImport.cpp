/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/imports/LinkImport.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportTreeRoot.h"
#include "core/html/imports/HTMLImportsController.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

LinkImport* LinkImport::Create(HTMLLinkElement* owner) {
  return new LinkImport(owner);
}

LinkImport::LinkImport(HTMLLinkElement* owner)
    : LinkResource(owner), child_(nullptr) {}

LinkImport::~LinkImport() {}

Document* LinkImport::ImportedDocument() const {
  if (!child_ || !owner_ || !owner_->isConnected())
    return nullptr;
  if (child_->Loader()->HasError())
    return nullptr;
  return child_->GetDocument();
}

void LinkImport::Process() {
  if (child_)
    return;
  if (!owner_)
    return;
  if (!ShouldLoadResource())
    return;

  if (!GetDocument().ImportsController()) {
    // The document should be the master.
    Document& master = GetDocument();
    DCHECK(master.GetFrame());
    master.CreateImportsController();
  }

  KURL url = owner_->GetNonEmptyURLAttribute(HTMLNames::hrefAttr);
  if (url.IsEmpty() || !url.IsValid()) {
    DidFinish();
    return;
  }

  HTMLImportsController* controller = GetDocument().ImportsController();
  HTMLImportLoader* loader = GetDocument().ImportLoader();
  HTMLImport* parent = loader ? static_cast<HTMLImport*>(loader->FirstImport())
                              : static_cast<HTMLImport*>(controller->Root());

  ResourceRequest resource_request(GetDocument().CompleteURL(url));
  ReferrerPolicy referrer_policy = owner_->GetReferrerPolicy();
  if (referrer_policy != kReferrerPolicyDefault) {
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        referrer_policy, url, GetDocument().OutgoingReferrer()));
  }

  ResourceLoaderOptions options;
  options.initiator_info.name = owner_->localName();
  FetchParameters params(resource_request, options);
  params.SetCharset(GetCharset());
  params.SetContentSecurityPolicyNonce(owner_->nonce());

  child_ = controller->Load(parent, this, params);
  if (!child_) {
    DidFinish();
    return;
  }
}

void LinkImport::DidFinish() {
  if (!owner_ || !owner_->isConnected())
    return;
  owner_->ScheduleEvent();
}

void LinkImport::ImportChildWasDisposed(HTMLImportChild* child) {
  DCHECK_EQ(child_, child);
  child_ = nullptr;
  owner_ = nullptr;
}

bool LinkImport::IsSync() const {
  return owner_ && !owner_->Async();
}

HTMLLinkElement* LinkImport::Link() {
  return owner_;
}

bool LinkImport::HasLoaded() const {
  return owner_ && child_ && child_->HasFinishedLoading() &&
         !child_->Loader()->HasError();
}

void LinkImport::OwnerInserted() {
  if (child_)
    child_->OwnerInserted();
}

void LinkImport::OwnerRemoved() {
  if (owner_)
    GetDocument().GetStyleEngine().HtmlImportAddedOrRemoved();
}

DEFINE_TRACE(LinkImport) {
  visitor->Trace(child_);
  HTMLImportChildClient::Trace(visitor);
  LinkResource::Trace(visitor);
}

}  // namespace blink
