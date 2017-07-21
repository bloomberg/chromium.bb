// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionCallbacks_h
#define PresentationConnectionCallbacks_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"

namespace blink {

class PresentationConnection;
class PresentationRequest;
class ScriptPromiseResolver;
struct WebPresentationError;
struct WebPresentationInfo;

// PresentationConnectionCallbacks extends WebCallbacks to resolve the
// underlying promise depending on the result passed to the callback. It takes
// the PresentationRequest object that originated the call in its constructor
// and will pass it to the created PresentationConnection.
class PresentationConnectionCallbacks final
    : public WebPresentationConnectionCallbacks {
 public:
  PresentationConnectionCallbacks(ScriptPromiseResolver*, PresentationRequest*);
  PresentationConnectionCallbacks(ScriptPromiseResolver*,
                                  PresentationConnection*);
  ~PresentationConnectionCallbacks() override = default;

  // WebCallbacks implementation
  void OnSuccess(const WebPresentationInfo&) override;
  void OnError(const WebPresentationError&) override;

  // WebPresentationConnectionCallbacks implementation
  WebPresentationConnection* GetConnection() override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  Persistent<PresentationRequest> request_;
  WeakPersistent<PresentationConnection> connection_;

  WTF_MAKE_NONCOPYABLE(PresentationConnectionCallbacks);
};

}  // namespace blink

#endif  // PresentationConnectionCallbacks_h
