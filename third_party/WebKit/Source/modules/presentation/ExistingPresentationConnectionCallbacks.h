// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExistingPresentationConnectionCallbacks_h
#define ExistingPresentationConnectionCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PresentationConnection;
class ScriptPromiseResolver;
struct WebPresentationError;
struct WebPresentationSessionInfo;

// ExistingPresentationConnectionCallbacks extends WebCallbacks to resolve the
// underlying promise. It takes the PresentationConnection object that
// originated the call in its constructor and will resolve underlying promise
// with that object.
// TODO(crbug.com/684111): Combine ExistingPresentationConnectionCallbacks with
// PresentationConnectionCallbacks
class ExistingPresentationConnectionCallbacks final
    : public WebPresentationConnectionCallbacks {
 public:
  ExistingPresentationConnectionCallbacks(ScriptPromiseResolver*,
                                          PresentationConnection*);
  ~ExistingPresentationConnectionCallbacks() override = default;

  // WebCallbacks implementation
  void onSuccess(const WebPresentationSessionInfo&) override;
  void onError(const WebPresentationError&) override;

  // WebPresentationConnectionCallbacks implementation
  WebPresentationConnection* getConnection() override;

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
  Persistent<PresentationConnection> m_connection;

  WTF_MAKE_NONCOPYABLE(ExistingPresentationConnectionCallbacks);
};

}  // namespace blink

#endif  // ExistingPresentationConnectionCallbacks_h
