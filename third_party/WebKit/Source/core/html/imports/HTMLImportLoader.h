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

#ifndef HTMLImportLoader_h
#define HTMLImportLoader_h

#include <memory>
#include "core/dom/DocumentParserClient.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/wtf/Vector.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue;
class Document;
class DocumentWriter;
class HTMLImportChild;
class HTMLImportsController;

// Owning imported Document lifetime. It also implements ResourceClient through
// ResourceOwner to feed fetched bytes to the DocumentWriter of the imported
// document.  HTMLImportLoader is owned by HTMLImportsController.
class HTMLImportLoader final
    : public GarbageCollectedFinalized<HTMLImportLoader>,
      public ResourceOwner<RawResource>,
      public DocumentParserClient {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLImportLoader);

 public:
  enum State {
    kStateLoading,
    kStateWritten,
    kStateParsed,
    kStateLoaded,
    kStateError
  };

  static HTMLImportLoader* Create(HTMLImportsController* controller) {
    return new HTMLImportLoader(controller);
  }

  ~HTMLImportLoader() final;
  void Dispose();

  Document* GetDocument() const { return document_.Get(); }
  void AddImport(HTMLImportChild*);
  void RemoveImport(HTMLImportChild*);

  void MoveToFirst(HTMLImportChild*);
  HTMLImportChild* FirstImport() const { return imports_[0]; }
  bool IsFirstImport(const HTMLImportChild* child) const {
    return imports_.size() ? FirstImport() == child : false;
  }

  bool IsDone() const {
    return state_ == kStateLoaded || state_ == kStateError;
  }
  bool HasError() const { return state_ == kStateError; }
  bool ShouldBlockScriptExecution() const;

  void StartLoading(RawResource*);

  // Tells the loader that all of the import's stylesheets finished
  // loading.
  // Called by Document::didRemoveAllPendingStylesheet.
  void DidRemoveAllPendingStylesheet();

  V0CustomElementSyncMicrotaskQueue* MicrotaskQueue() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLImportLoader(HTMLImportsController*);

  // RawResourceClient overrides:
  void ResponseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) final;
  void DataReceived(Resource*, const char* data, size_t length) final;
  void NotifyFinished(Resource*) final;
  String DebugName() const final { return "HTMLImportLoader"; }

  // DocumentParserClient overrides:

  // Called after document parse is complete after DOMContentLoaded was
  // dispatched.
  void NotifyParserStopped() final;

  State StartWritingAndParsing(const ResourceResponse&);
  State FinishWriting();
  State FinishParsing();
  State FinishLoading();

  void SetState(State);
  void DidFinishLoading();
  bool HasPendingResources() const;

  Member<HTMLImportsController> controller_;
  HeapVector<Member<HTMLImportChild>> imports_;
  State state_;
  Member<Document> document_;
  Member<DocumentWriter> writer_;
  Member<V0CustomElementSyncMicrotaskQueue> microtask_queue_;
};

}  // namespace blink

#endif  // HTMLImportLoader_h
