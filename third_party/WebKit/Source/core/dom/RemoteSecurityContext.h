// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteSecurityContext_h
#define RemoteSecurityContext_h

#include "core/CoreExport.h"
#include "core/dom/SecurityContext.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT RemoteSecurityContext
    : public GarbageCollectedFinalized<RemoteSecurityContext>,
      public SecurityContext {
  USING_GARBAGE_COLLECTED_MIXIN(RemoteSecurityContext);

 public:
  virtual void Trace(blink::Visitor*);

  static RemoteSecurityContext* Create();
  void SetReplicatedOrigin(scoped_refptr<SecurityOrigin>);
  void ResetReplicatedContentSecurityPolicy();
  void ResetSandboxFlags();

  // FIXME: implement
  void DidUpdateSecurityOrigin() override {}

 private:
  RemoteSecurityContext();
};

}  // namespace blink

#endif  // RemoteSecurityContext_h
