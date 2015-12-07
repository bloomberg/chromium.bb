// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCClient_h
#define WebNFCClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"

namespace blink {

enum class WebNFCError;
struct WebNFCMessage;
class WebNFCObserver;
struct WebNFCPushOptions;
enum class WebNFCPushTarget;
struct WebNFCWatchOptions;

// Success and failure callbacks for push, cancelPush, cancelWatch and cancelAllWatches methods.
using WebNFCCallbacks = WebCallbacks<void, const WebNFCError&>;

// Success and failure callbacks for watch method.
using WebNFCWatchOptionsCallbacks = WebCallbacks<long, const WebNFCError&>;

class WebNFCClient {
public:
    virtual ~WebNFCClient() { }

    // Sets observer to the client so it can notify the observer about
    // triggered NFC watch events.
    virtual void setObserver(WebNFCObserver*) = 0;

    // NFC interface methods:

    // See https://w3c.github.io/web-nfc/#the-push-method
    // WebNFCCallbacks ownership is transfered to the client.
    virtual void push(const WebNFCMessage&, const WebNFCPushOptions&, WebNFCCallbacks*) = 0;

    // See https://w3c.github.io/web-nfc/#cancelPush
    // WebNFCCallbacks ownership is transfered to the client.
    virtual void cancelPush(const WebNFCPushTarget&, WebNFCCallbacks*) = 0;

    // See https://w3c.github.io/web-nfc/#the-watch-method
    // WebNFCWatchOptionsCallbacks ownership is transfered to the client.
    virtual void watch(const WebNFCWatchOptions&, WebNFCWatchOptionsCallbacks*) = 0;

    // See https://w3c.github.io/web-nfc/#the-cancelwatch-method
    // WebNFCCallbacks ownership is transfered to the client.
    virtual void cancelWatch(long id, WebNFCCallbacks*) = 0;
    virtual void cancelAllWatches(WebNFCCallbacks*) = 0;

    // Methods that are required to implement suspended state.
    // See https://w3c.github.io/web-nfc/#nfc-suspended
    virtual void suspendNFCOperations() = 0;
    virtual void resumeNFCOperations() = 0;
};

} // namespace blink

#endif // WebNFCClient_h
