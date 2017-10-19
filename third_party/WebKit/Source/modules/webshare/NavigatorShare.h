// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorShare_h
#define NavigatorShare_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "public/platform/modules/webshare/webshare.mojom-blink.h"

namespace blink {

class Navigator;
class ShareData;

class NavigatorShare final : public GarbageCollectedFinalized<NavigatorShare>,
                             public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorShare);

 public:
  ~NavigatorShare();

  // Gets, or creates, NavigatorShare supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorShare& From(Navigator&);

  // Navigator partial interface
  ScriptPromise share(ScriptState*, const ShareData&);
  static ScriptPromise share(ScriptState*, Navigator&, const ShareData&);

  void Trace(blink::Visitor*);

 private:
  class ShareClientImpl;

  NavigatorShare();
  static const char* SupplementName();

  void OnConnectionError();

  blink::mojom::blink::ShareServicePtr service_;

  HeapHashSet<Member<ShareClientImpl>> clients_;
};

}  // namespace blink

#endif  // NavigatorShare_h
