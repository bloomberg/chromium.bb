// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerWindowClientCallback_h
#define ServiceWorkerWindowClientCallback_h

#include "base/macros.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"

namespace blink {

class ScriptPromiseResolver;

class NavigateClientCallback : public WebServiceWorkerClientCallbacks {
 public:
  explicit NavigateClientCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(std::unique_ptr<WebServiceWorkerClientInfo>) override;
  void OnError(const WebServiceWorkerError&) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  DISALLOW_COPY_AND_ASSIGN(NavigateClientCallback);
};

}  // namespace blink

#endif  // ServiceWorkerWindowClientCallback_h
