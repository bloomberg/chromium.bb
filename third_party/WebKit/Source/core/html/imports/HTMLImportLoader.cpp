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

#include "core/html/imports/HTMLImportLoader.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"
#include "core/html/HTMLDocument.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/loader/DocumentWriter.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include <memory>

namespace blink {

HTMLImportLoader::HTMLImportLoader(HTMLImportsController* controller)
    : controller_(controller),
      state_(kStateLoading),
      microtask_queue_(V0CustomElementSyncMicrotaskQueue::Create()) {}

HTMLImportLoader::~HTMLImportLoader() {}

void HTMLImportLoader::Dispose() {
  controller_ = nullptr;
  if (document_) {
    if (document_->Parser())
      document_->Parser()->RemoveClient(this);
    document_->ClearImportsController();
    document_.Clear();
  }
  ClearResource();
}

void HTMLImportLoader::StartLoading(RawResource* resource) {
  SetResource(resource);
}

void HTMLImportLoader::ResponseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  // Resource may already have been loaded with the import loader
  // being added as a client later & now being notified. Fail early.
  if (resource->LoadFailedOrCanceled() || response.HttpStatusCode() >= 400 ||
      !response.HttpHeaderField(HTTPNames::Content_Disposition).IsNull()) {
    SetState(kStateError);
    return;
  }
  SetState(StartWritingAndParsing(response));
}

void HTMLImportLoader::DataReceived(Resource*,
                                    const char* data,
                                    size_t length) {
  writer_->AddData(data, length);
}

void HTMLImportLoader::NotifyFinished(Resource* resource) {
  // The writer instance indicates that a part of the document can be already
  // loaded.  We don't take such a case as an error because the partially-loaded
  // document has been visible from script at this point.
  if (resource->LoadFailedOrCanceled() && !writer_) {
    SetState(kStateError);
    return;
  }

  SetState(FinishWriting());
}

HTMLImportLoader::State HTMLImportLoader::StartWritingAndParsing(
    const ResourceResponse& response) {
  DCHECK(controller_);
  DCHECK(!imports_.IsEmpty());
  DocumentInit init =
      DocumentInit(response.Url(), 0, controller_->Master()->ContextDocument(),
                   controller_)
          .WithRegistrationContext(
              controller_->Master()->RegistrationContext());
  document_ = HTMLDocument::Create(init);
  writer_ = DocumentWriter::Create(document_.Get(), kAllowAsynchronousParsing,
                                   response.MimeType(), "UTF-8");

  DocumentParser* parser = document_->Parser();
  DCHECK(parser);
  parser->AddClient(this);

  return kStateLoading;
}

HTMLImportLoader::State HTMLImportLoader::FinishWriting() {
  return kStateWritten;
}

HTMLImportLoader::State HTMLImportLoader::FinishParsing() {
  return kStateParsed;
}

HTMLImportLoader::State HTMLImportLoader::FinishLoading() {
  return kStateLoaded;
}

void HTMLImportLoader::SetState(State state) {
  if (state_ == state)
    return;

  state_ = state;

  if (state_ == kStateParsed || state_ == kStateError ||
      state_ == kStateWritten) {
    if (DocumentWriter* writer = writer_.Release())
      writer->end();
  }

  // Since DocumentWriter::end() can let setState() reenter, we shouldn't refer
  // to m_state here.
  if (state == kStateLoaded)
    document_->SetReadyState(Document::kComplete);
  if (state == kStateLoaded || state == kStateError)
    DidFinishLoading();
}

void HTMLImportLoader::NotifyParserStopped() {
  SetState(FinishParsing());
  if (!HasPendingResources())
    SetState(FinishLoading());

  DocumentParser* parser = document_->Parser();
  DCHECK(parser);
  parser->RemoveClient(this);
}

void HTMLImportLoader::DidRemoveAllPendingStylesheet() {
  if (state_ == kStateParsed)
    SetState(FinishLoading());
}

bool HTMLImportLoader::HasPendingResources() const {
  return document_ &&
         document_->GetStyleEngine().HasPendingScriptBlockingSheets();
}

void HTMLImportLoader::DidFinishLoading() {
  for (const auto& import_child : imports_)
    import_child->DidFinishLoading();

  ClearResource();

  DCHECK(!document_ || !document_->Parsing());
}

void HTMLImportLoader::MoveToFirst(HTMLImportChild* import) {
  size_t position = imports_.Find(import);
  DCHECK_NE(kNotFound, position);
  imports_.erase(position);
  imports_.insert(0, import);
}

void HTMLImportLoader::AddImport(HTMLImportChild* import) {
  DCHECK_EQ(kNotFound, imports_.Find(import));

  imports_.push_back(import);
  import->Normalize();
  if (IsDone())
    import->DidFinishLoading();
}

void HTMLImportLoader::RemoveImport(HTMLImportChild* client) {
  DCHECK_NE(kNotFound, imports_.Find(client));
  imports_.erase(imports_.Find(client));
}

bool HTMLImportLoader::ShouldBlockScriptExecution() const {
  return FirstImport()->GetState().ShouldBlockScriptExecution();
}

V0CustomElementSyncMicrotaskQueue* HTMLImportLoader::MicrotaskQueue() const {
  return microtask_queue_;
}

DEFINE_TRACE(HTMLImportLoader) {
  visitor->Trace(controller_);
  visitor->Trace(imports_);
  visitor->Trace(document_);
  visitor->Trace(writer_);
  visitor->Trace(microtask_queue_);
  DocumentParserClient::Trace(visitor);
  ResourceOwner<RawResource>::Trace(visitor);
}

}  // namespace blink
