// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionCallbacks_h
#define PresentationConnectionCallbacks_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace blink {

class ControllerPresentationConnection;
class PresentationRequest;
class ScriptPromiseResolver;

// PresentationConnectionCallbacks resolves or rejects the provided resolver's
// underlying promise depending on the result passed to the callback. On
// success, the promise will be resolved with a newly created
// ControllerPresentationConnection. In the case of reconnect, the callback may
// take an existing connection object with which the promise will be resolved
// on success.
class PresentationConnectionCallbacks final {
 public:
  PresentationConnectionCallbacks(ScriptPromiseResolver*, PresentationRequest*);
  PresentationConnectionCallbacks(ScriptPromiseResolver*,
                                  ControllerPresentationConnection*);
  ~PresentationConnectionCallbacks() = default;

  void HandlePresentationResponse(mojom::blink::PresentationInfoPtr,
                                  mojom::blink::PresentationErrorPtr);

 private:
  void OnSuccess(const mojom::blink::PresentationInfo&);
  void OnError(const mojom::blink::PresentationError&);

  Persistent<ScriptPromiseResolver> resolver_;
  Persistent<PresentationRequest> request_;
  WeakPersistent<ControllerPresentationConnection> connection_;

  WTF_MAKE_NONCOPYABLE(PresentationConnectionCallbacks);
};

}  // namespace blink

#endif  // PresentationConnectionCallbacks_h
