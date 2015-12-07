// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCObserver_h
#define WebNFCObserver_h

namespace blink {

struct WebNFCMessage;

// Observer interface that should be implemented in order to get notification
// about triggered NFC watch.
class WebNFCObserver {
public:
    virtual ~WebNFCObserver() { }

    // The id is an identifier of the registered NFC watch that was obtained after
    // calling watch method (https://w3c.github.io/web-nfc/#the-watch-method).
    // The WebNFCMessage that matches NFC watch filtering criteria is provided to the callback.
    // See https://w3c.github.io/web-nfc/#idl-def-messagecallback
    virtual void onWatch(long id, const WebNFCMessage&) = 0;
};

} // namespace blink

#endif // WebNFCObserver_h
