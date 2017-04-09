// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerWindowClient_h
#define ServiceWorkerWindowClient_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT ServiceWorkerWindowClient final
    : public ServiceWorkerClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // To be used by CallbackPromiseAdapter.
  using WebType = std::unique_ptr<WebServiceWorkerClientInfo>;

  static ServiceWorkerWindowClient* Take(
      ScriptPromiseResolver*,
      std::unique_ptr<WebServiceWorkerClientInfo>);

  static ServiceWorkerWindowClient* Create(const WebServiceWorkerClientInfo&);
  ~ServiceWorkerWindowClient() override;

  // WindowClient.idl
  String visibilityState() const;
  bool focused() const { return is_focused_; }
  ScriptPromise focus(ScriptState*);
  ScriptPromise navigate(ScriptState*, const String& url);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit ServiceWorkerWindowClient(const WebServiceWorkerClientInfo&);

  WebPageVisibilityState page_visibility_state_;
  bool is_focused_;
};

}  // namespace blink

#endif  // ServiceWorkerWindowClient_h
