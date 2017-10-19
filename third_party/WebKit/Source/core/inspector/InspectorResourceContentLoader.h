// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContentLoader_h
#define InspectorResourceContentLoader_h

#include "core/CoreExport.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class KURL;
class LocalFrame;
class Resource;

class CORE_EXPORT InspectorResourceContentLoader final
    : public GarbageCollectedFinalized<InspectorResourceContentLoader> {
  WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);

 public:
  static InspectorResourceContentLoader* Create(LocalFrame* inspected_frame) {
    return new InspectorResourceContentLoader(inspected_frame);
  }
  ~InspectorResourceContentLoader();
  void Dispose();
  void Trace(blink::Visitor*);

  int CreateClientId();
  void EnsureResourcesContentLoaded(int client_id, WTF::Closure callback);
  void Cancel(int client_id);
  void DidCommitLoadForLocalFrame(LocalFrame*);

  Resource* ResourceForURL(const KURL&);

 private:
  class ResourceClient;

  explicit InspectorResourceContentLoader(LocalFrame*);
  void ResourceFinished(ResourceClient*);
  void CheckDone();
  void Start();
  void Stop();
  bool HasFinished();

  using Callbacks = Vector<WTF::Closure>;
  HashMap<int, Callbacks> callbacks_;
  bool all_requests_started_;
  bool started_;
  Member<LocalFrame> inspected_frame_;
  HeapHashSet<Member<ResourceClient>> pending_resource_clients_;
  HeapVector<Member<Resource>> resources_;
  int last_client_id_;

  friend class ResourceClient;
};

}  // namespace blink

#endif  // !defined(InspectorResourceContentLoader_h)
