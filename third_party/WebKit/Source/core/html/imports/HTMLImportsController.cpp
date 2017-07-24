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

#include "core/html/imports/HTMLImportsController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportChildClient.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportTreeRoot.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

HTMLImportsController::HTMLImportsController(Document& master)
    : root_(this, HTMLImportTreeRoot::Create(&master)) {
  UseCounter::Count(master, WebFeature::kHTMLImports);
}

void HTMLImportsController::Dispose() {
  for (const auto& loader : loaders_)
    loader->Dispose();
  loaders_.clear();

  if (root_) {
    root_->Dispose();
    root_.Clear();
  }
}

static bool MakesCycle(HTMLImport* parent, const KURL& url) {
  for (HTMLImport* ancestor = parent; ancestor; ancestor = ancestor->Parent()) {
    if (!ancestor->IsRoot() &&
        EqualIgnoringFragmentIdentifier(ToHTMLImportChild(parent)->Url(), url))
      return true;
  }

  return false;
}

HTMLImportChild* HTMLImportsController::CreateChild(
    const KURL& url,
    HTMLImportLoader* loader,
    HTMLImport* parent,
    HTMLImportChildClient* client) {
  HTMLImport::SyncMode mode = client->IsSync() && !MakesCycle(parent, url)
                                  ? HTMLImport::kSync
                                  : HTMLImport::kAsync;
  if (mode == HTMLImport::kAsync) {
    UseCounter::Count(Root()->GetDocument(),
                      WebFeature::kHTMLImportsAsyncAttribute);
  }

  HTMLImportChild* child = new HTMLImportChild(url, loader, mode);
  child->SetClient(client);
  parent->AppendImport(child);
  loader->AddImport(child);
  return Root()->Add(child);
}

HTMLImportChild* HTMLImportsController::Load(HTMLImport* parent,
                                             HTMLImportChildClient* client,
                                             FetchParameters& params) {
  const KURL& url = params.Url();

  DCHECK(!url.IsEmpty());
  DCHECK(url.IsValid());
  DCHECK(parent == Root() || ToHTMLImportChild(parent)->Loader()->IsFirstImport(
                                 ToHTMLImportChild(parent)));

  if (HTMLImportChild* child_to_share_with = Root()->Find(url)) {
    HTMLImportLoader* loader = child_to_share_with->Loader();
    DCHECK(loader);
    HTMLImportChild* child = CreateChild(url, loader, parent, client);
    child->DidShareLoader();
    return child;
  }

  params.SetCrossOriginAccessControl(Master()->GetSecurityOrigin(),
                                     kCrossOriginAttributeAnonymous);
  RawResource* resource =
      RawResource::FetchImport(params, parent->GetDocument()->Fetcher());
  if (!resource)
    return nullptr;

  HTMLImportLoader* loader = CreateLoader();
  HTMLImportChild* child = CreateChild(url, loader, parent, client);
  // We set resource after the import tree is built since
  // Resource::addClient() immediately calls back to feed the bytes when the
  // resource is cached.
  loader->StartLoading(resource);
  child->DidStartLoading();
  return child;
}

Document* HTMLImportsController::Master() const {
  return Root() ? Root()->GetDocument() : nullptr;
}

bool HTMLImportsController::ShouldBlockScriptExecution(
    const Document& document) const {
  DCHECK_EQ(document.ImportsController(), this);
  if (HTMLImportLoader* loader = LoaderFor(document))
    return loader->ShouldBlockScriptExecution();
  return Root()->GetState().ShouldBlockScriptExecution();
}

HTMLImportLoader* HTMLImportsController::CreateLoader() {
  loaders_.push_back(HTMLImportLoader::Create(this));
  return loaders_.back().Get();
}

HTMLImportLoader* HTMLImportsController::LoaderFor(
    const Document& document) const {
  for (const auto& loader : loaders_) {
    if (loader->GetDocument() == &document)
      return loader.Get();
  }

  return nullptr;
}

DEFINE_TRACE(HTMLImportsController) {
  visitor->Trace(root_);
  visitor->Trace(loaders_);
}

DEFINE_TRACE_WRAPPERS(HTMLImportsController) {
  visitor->TraceWrappers(root_);
}

}  // namespace blink
