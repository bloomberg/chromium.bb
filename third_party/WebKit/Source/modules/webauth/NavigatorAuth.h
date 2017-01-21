// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorAuth_h
#define NavigatorAuth_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class WebAuthentication;

class NavigatorAuth final : public GarbageCollected<NavigatorAuth>,
                            public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorAuth);

 public:
  static NavigatorAuth& from(Navigator&);

  static WebAuthentication* authentication(Navigator&);
  WebAuthentication* authentication();

  DECLARE_TRACE();

 private:
  explicit NavigatorAuth(Navigator&);
  static const char* supplementName();

  Member<WebAuthentication> m_webauthentication;
};

}  // namespace blink

#endif  // NavigatorAuth_h
