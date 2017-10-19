// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorKeyboardLock_h
#define NavigatorKeyboardLock_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Member.h"
#include "platform/wtf/Forward.h"
#include "public/platform/modules/keyboard_lock/keyboard_lock.mojom-blink.h"

namespace blink {

class ScriptPromiseResolver;

// The supplement of Navigator to process navigator.requestKeyboardLock() and
// navigator.cancelKeyboardLock() web APIs. This class forwards both requests
// directly to the browser process through mojo.
class NavigatorKeyboardLock final
    : public GarbageCollectedFinalized<NavigatorKeyboardLock>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorKeyboardLock);

 public:
  // Requests to receive a set of key codes
  // (https://w3c.github.io/uievents/#dom-keyboardevent-code) in string format.
  // The Promise will be rejected if the user or browser do not allow the web
  // page to use this API. Otherwise, web page should expect to receive the key
  // presses once the Promise has been resolved. This API does best effort to
  // deliver the key codes: due to the platform restrictions, some keys or key
  // combinations may not be able to receive or intercept by the user agent.
  // - Making two requests concurrently without waiting for one Promise to
  //   finish is disallowed, the second Promise will be rejected immediately.
  // - Making a second request after the Promise of the first one has finished
  //   is allowed; the second request will overwrite the key codes reserved.
  // - Passing in an empty keyCodes array will reserve all keys.
  static ScriptPromise requestKeyboardLock(ScriptState*,
                                      Navigator&,
                                      const Vector<String>&);

  // Removes all reserved keys. This function is also asynchronized, the web
  // page may still receive reserved keys after this function has finished. Once
  // the web page is closed, the user agent implicitly executes this API.
  static void cancelKeyboardLock(Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorKeyboardLock(Navigator&);
  static const char* SupplementName();

  static NavigatorKeyboardLock& From(Navigator&);

  ScriptPromise requestKeyboardLock(ScriptState*, const Vector<String>&);
  void cancelKeyboardLock();

  // Ensures the |service_| is correctly initialized. In case of the |service_|
  // cannot be initialized, this function returns false.
  bool EnsureServiceConnected();

  void LockRequestFinished(mojom::KeyboardLockRequestResult);

  mojom::blink::KeyboardLockServicePtr service_;
  Member<ScriptPromiseResolver> request_keylock_resolver_;
};

}  // namespace blink

#endif  // NavigatorKeyboardLock_h
