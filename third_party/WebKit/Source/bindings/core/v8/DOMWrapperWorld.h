/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef DOMWrapperWorld_h
#define DOMWrapperWorld_h

#include <memory>

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "v8/include/v8.h"

namespace blink {

class DOMDataStore;
class DOMObjectHolderBase;

// This class represent a collection of DOM wrappers for a specific world. This
// is identified by a world id that is a per-thread global identifier (see
// WorldId enum).
class CORE_EXPORT DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
 public:
  // Per-thread global identifiers for DOMWrapperWorld.
  enum WorldId {
    kInvalidWorldId = -1,
    kMainWorldId = 0,

    // Embedder isolated worlds can use IDs in [1, 1<<29).
    kEmbedderWorldIdLimit = (1 << 29),
    kDocumentXMLTreeViewerWorldId,
    kIsolatedWorldIdLimit,

    // Other worlds can use IDs after this. Don't manually pick up an ID from
    // this range. generateWorldIdForType() picks it up on behalf of you.
    kUnspecifiedWorldIdStart,
  };

  enum class WorldType {
    kMain,
    kIsolated,
    kGarbageCollector,
    kRegExp,
    kTesting,
    kWorker,
  };

  // Creates a world other than IsolatedWorld.
  static PassRefPtr<DOMWrapperWorld> Create(v8::Isolate*, WorldType);

  // Ensures an IsolatedWorld for |worldId|.
  static PassRefPtr<DOMWrapperWorld> EnsureIsolatedWorld(v8::Isolate*,
                                                         int world_id);
  ~DOMWrapperWorld();
  void Dispose();

  // Called from performance-sensitive functions, so we should keep this simple
  // and fast as much as possible.
  static bool NonMainWorldsExistInMainThread() {
    return number_of_non_main_worlds_in_main_thread_;
  }

  static void AllWorldsInCurrentThread(Vector<RefPtr<DOMWrapperWorld>>& worlds);
  static void MarkWrappersInAllWorlds(ScriptWrappable*,
                                      const ScriptWrappableVisitor*);

  static DOMWrapperWorld& World(v8::Local<v8::Context> context) {
    return ScriptState::From(context)->World();
  }

  static DOMWrapperWorld& Current(v8::Isolate* isolate) {
    return World(isolate->GetCurrentContext());
  }

  static DOMWrapperWorld& MainWorld();

  static void SetIsolatedWorldHumanReadableName(int world_id, const String&);
  String IsolatedWorldHumanReadableName();

  // Associates an isolated world (see above for description) with a security
  // origin. XMLHttpRequest instances used in that world will be considered
  // to come from that origin, not the frame's.
  static void SetIsolatedWorldSecurityOrigin(int world_id,
                                             PassRefPtr<SecurityOrigin>);
  SecurityOrigin* IsolatedWorldSecurityOrigin();

  // Associated an isolated world with a Content Security Policy. Resources
  // embedded into the main world's DOM from script executed in an isolated
  // world should be restricted based on the isolated world's DOM, not the
  // main world's.
  //
  // FIXME: Right now, resource injection simply bypasses the main world's
  // DOM. More work is necessary to allow the isolated world's policy to be
  // applied correctly.
  static void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                                    const String& policy);
  bool IsolatedWorldHasContentSecurityPolicy();

  bool IsMainWorld() const { return world_type_ == WorldType::kMain; }
  bool IsWorkerWorld() const { return world_type_ == WorldType::kWorker; }
  bool IsIsolatedWorld() const { return world_type_ == WorldType::kIsolated; }

  int GetWorldId() const { return world_id_; }
  DOMDataStore& DomDataStore() const { return *dom_data_store_; }

 public:
  template <typename T>
  void RegisterDOMObjectHolder(v8::Isolate*, T*, v8::Local<v8::Value>);

 private:
  DOMWrapperWorld(v8::Isolate*, WorldType, int world_id);

  static void WeakCallbackForDOMObjectHolder(
      const v8::WeakCallbackInfo<DOMObjectHolderBase>&);
  void RegisterDOMObjectHolderInternal(std::unique_ptr<DOMObjectHolderBase>);
  void UnregisterDOMObjectHolder(DOMObjectHolderBase*);

  static unsigned number_of_non_main_worlds_in_main_thread_;

  // Returns an identifier for a given world type. This must not be called for
  // WorldType::IsolatedWorld because an identifier for the world is given from
  // out of DOMWrapperWorld.
  static int GenerateWorldIdForType(WorldType);

  const WorldType world_type_;
  const int world_id_;
  std::unique_ptr<DOMDataStore> dom_data_store_;
  HashSet<std::unique_ptr<DOMObjectHolderBase>> dom_object_holders_;
};

}  // namespace blink

#endif  // DOMWrapperWorld_h
