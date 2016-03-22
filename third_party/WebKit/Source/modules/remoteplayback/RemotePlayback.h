// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlayback_h
#define RemotePlayback_h

#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class HTMLMediaElement;
class LocalFrame;

class RemotePlayback final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<RemotePlayback>
    , public DOMWindowProperty
    , private WebRemotePlaybackClient {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(RemotePlayback);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RemotePlayback);
public:
    static RemotePlayback* create(HTMLMediaElement&);

    ~RemotePlayback() override = default;

    // EventTarget implementation.
    const WTF::AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    String state() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    DECLARE_VIRTUAL_TRACE();

private:
    RemotePlayback(LocalFrame*, WebRemotePlaybackState);

    void stateChanged(WebRemotePlaybackState) override;

    WebRemotePlaybackState m_state;
};

} // namespace blink

#endif // RemotePlayback_h
