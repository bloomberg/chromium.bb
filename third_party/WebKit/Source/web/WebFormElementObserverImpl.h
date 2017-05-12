// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFormElementObserverImpl_h
#define WebFormElementObserverImpl_h

#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Member.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Compiler.h"
#include "public/web/modules/password_manager/WebFormElementObserver.h"
#include "web/WebExport.h"

namespace blink {

class HTMLElement;
class WebFormElementObserverCallback;

class WEB_EXPORT WebFormElementObserverImpl final
    : public GarbageCollectedFinalized<WebFormElementObserverImpl>,
      NON_EXPORTED_BASE(public WebFormElementObserver) {
  WTF_MAKE_NONCOPYABLE(WebFormElementObserverImpl);

 public:
  WebFormElementObserverImpl(HTMLElement&,
                             std::unique_ptr<WebFormElementObserverCallback>);
  ~WebFormElementObserverImpl() override;

  // WebFormElementObserver implementation.
  void Disconnect() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  class ObserverCallback;

  Member<ObserverCallback> mutation_callback_;

  // WebFormElementObserverImpl must remain alive until Disconnect() is called.
  SelfKeepAlive<WebFormElementObserverImpl> self_keep_alive_;
};

}  // namespace blink

#endif  // WebFormElementObserverImpl_h
