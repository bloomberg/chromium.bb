// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigationPreloadCallbacks_h
#define NavigationPreloadCallbacks_h

#include "platform/heap/Persistent.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {

class ScriptPromiseResolver;
struct WebNavigationPreloadState;
struct WebServiceWorkerError;

class EnableNavigationPreloadCallbacks final
    : public WebServiceWorkerRegistration::WebEnableNavigationPreloadCallbacks {
 public:
  explicit EnableNavigationPreloadCallbacks(ScriptPromiseResolver*);
  ~EnableNavigationPreloadCallbacks() override;

  // WebEnableNavigationPreloadCallbacks interface.
  void OnSuccess() override;
  void OnError(const WebServiceWorkerError&) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  WTF_MAKE_NONCOPYABLE(EnableNavigationPreloadCallbacks);
};

class GetNavigationPreloadStateCallbacks final
    : public WebServiceWorkerRegistration::
          WebGetNavigationPreloadStateCallbacks {
 public:
  explicit GetNavigationPreloadStateCallbacks(ScriptPromiseResolver*);
  ~GetNavigationPreloadStateCallbacks() override;

  // WebGetNavigationPreloadStateCallbacks interface.
  void OnSuccess(const WebNavigationPreloadState&) override;
  void OnError(const WebServiceWorkerError&) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  WTF_MAKE_NONCOPYABLE(GetNavigationPreloadStateCallbacks);
};

class SetNavigationPreloadHeaderCallbacks final
    : public WebServiceWorkerRegistration::
          WebSetNavigationPreloadHeaderCallbacks {
 public:
  explicit SetNavigationPreloadHeaderCallbacks(ScriptPromiseResolver*);
  ~SetNavigationPreloadHeaderCallbacks() override;

  // WebSetNavigationPreloadHeaderCallbacks interface.
  void OnSuccess(void) override;
  void OnError(const WebServiceWorkerError&) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  WTF_MAKE_NONCOPYABLE(SetNavigationPreloadHeaderCallbacks);
};

}  // namespace blink

#endif  // NavigationPreloadCallbacks_h
