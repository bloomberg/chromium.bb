// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentWriteIntervention_h
#define DocumentWriteIntervention_h

#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceOwner.h"

// document.write() intervention may
// - block network loading of a script inserted by document.write() and
// - send an asynchronous GET request to the blocked URL (with an
//   intervention header) that doesn't cause script execution, in order to:
//   - Notify the page authors, and
//   - Fill in the disk cache for a future use.
//   This also fills in the MemoryCache, but the ScriptResource will be GCed
//   and removed from the MemoryCache very soon (it's OK to reuse the
//   ScriptResource, but we don't have to keep it on MemoryCache).
// https://developers.google.com/web/updates/2016/08/removing-document-write

namespace blink {

class Document;

// Created and AddClient()ed if the script MAY BE blocked, and when
// NotifyFinished(), outputs console errors/warnings depending on whether
// the script is actually blocked or not, and sends an asynchronous GET
// request with an interventions header if blocked.
// Note that this might occur just before or just after the corresponding
// <script>'s execution/onload/onerror.
class FetchBlockedDocWriteScriptClient
    : public GarbageCollectedFinalized<FetchBlockedDocWriteScriptClient>,
      public ResourceOwner<ScriptResource> {
  USING_GARBAGE_COLLECTED_MIXIN(FetchBlockedDocWriteScriptClient);

 public:
  FetchBlockedDocWriteScriptClient(Document& document,
                                   const FetchParameters& params)
      : document_(&document), params_(params) {}

  void SetResource(ScriptResource*);

  void Trace(Visitor*) override;

 private:
  void NotifyFinished(Resource*) override;
  String DebugName() const override {
    return "FetchBlockedDocWriteScriptClient";
  }

  WeakMember<Document> document_;
  FetchParameters params_;
  SelfKeepAlive<FetchBlockedDocWriteScriptClient> keep_alive_;
};

// Returns non-null if the fetch should be blocked due to the document.write
// intervention. In that case, the request's cache policy is set to
// kReturnCacheDataDontLoad to ensure a network request is not generated.
// This function may also set an Intervention header, log the intervention in
// the console, etc.
//
// This only affects scripts added via document.write() in the main frame.
//
// The caller should call SetResource() for the returned client.
FetchBlockedDocWriteScriptClient* MaybeDisallowFetchForDocWrittenScript(
    FetchParameters&,
    Document&);

}  // namespace blink

#endif  // DocumentWriteIntervention_h
