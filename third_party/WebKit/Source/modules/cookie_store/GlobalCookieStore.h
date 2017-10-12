// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalCookieStore_h
#define GlobalCookieStore_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CookieStore;
class LocalDOMWindow;

// Exposes a CookieStore as the "cookieStore" attribute on the global scope.
//
// This currently applies to Window scopes.
class GlobalCookieStore {
  STATIC_ONLY(GlobalCookieStore);

 public:
  static CookieStore* cookieStore(LocalDOMWindow&);

  // TODO(crbug.com/729800): Expose to ServiceWorkerGlobalScope.
};

}  // namespace blink

#endif  // GlobalCookieStore_h
