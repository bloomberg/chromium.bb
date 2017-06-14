// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorContentUtilsClient_h
#define NavigatorContentUtilsClient_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class WebLocalFrameBase;

class MODULES_EXPORT NavigatorContentUtilsClient
    : public GarbageCollectedFinalized<NavigatorContentUtilsClient> {
 public:
  static NavigatorContentUtilsClient* Create(WebLocalFrameBase*);
  virtual ~NavigatorContentUtilsClient() {}

  virtual void RegisterProtocolHandler(const String& scheme,
                                       const KURL&,
                                       const String& title);

  enum CustomHandlersState {
    kCustomHandlersNew,
    kCustomHandlersRegistered,
    kCustomHandlersDeclined
  };

  virtual CustomHandlersState IsProtocolHandlerRegistered(const String& scheme,
                                                          const KURL&);
  virtual void UnregisterProtocolHandler(const String& scheme, const KURL&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit NavigatorContentUtilsClient(WebLocalFrameBase*);

 private:
  Member<WebLocalFrameBase> web_frame_;
};

}  // namespace blink

#endif  // NavigatorContentUtilsClient_h
