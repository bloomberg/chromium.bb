// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorContentUtilsClientImpl_h
#define NavigatorContentUtilsClientImpl_h

#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class WebLocalFrameBase;

class NavigatorContentUtilsClientImpl final
    : public NavigatorContentUtilsClient {
 public:
  static NavigatorContentUtilsClientImpl* Create(WebLocalFrameBase*);
  ~NavigatorContentUtilsClientImpl() override {}

  void RegisterProtocolHandler(const String& scheme,
                               const KURL&,
                               const String& title) override;
  CustomHandlersState IsProtocolHandlerRegistered(const String& scheme,
                                                  const KURL&) override;
  void UnregisterProtocolHandler(const String& scheme, const KURL&) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit NavigatorContentUtilsClientImpl(WebLocalFrameBase*);

  Member<WebLocalFrameBase> web_frame_;
};

}  // namespace blink

#endif  // NavigatorContentUtilsClientImpl_h
