// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/DOMWindow.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SecurityContext.h"
#include "core/events/MessageEvent.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameClient.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Location.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/MixedContentChecker.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

DOMWindow::~DOMWindow()
{
}

Location* DOMWindow::location() const
{
    if (!m_location)
        m_location = Location::create(frame());
    return m_location.get();
}

bool DOMWindow::closed() const
{
    return !frame() || !frame()->host();
}

unsigned DOMWindow::length() const
{
    return frame() ? frame()->tree().scopedChildCount() : 0;
}

DOMWindow* DOMWindow::self() const
{
    if (!frame())
        return nullptr;

    return frame()->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    // FIXME: Use FrameTree to get opener as well, to simplify logic here.
    if (!frame() || !frame()->client())
        return nullptr;

    Frame* opener = frame()->client()->opener();
    return opener ? opener->domWindow() : nullptr;
}

DOMWindow* DOMWindow::parent() const
{
    if (!frame())
        return nullptr;

    Frame* parent = frame()->tree().parent();
    return parent ? parent->domWindow() : frame()->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!frame())
        return nullptr;

    return frame()->tree().top()->domWindow();
}

DOMWindow* DOMWindow::anonymousIndexedGetter(uint32_t index) const
{
    if (!frame())
        return nullptr;

    Frame* child = frame()->tree().scopedChild(index);
    return child ? child->domWindow() : nullptr;
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    return frame() && frame()->domWindow() == this && frame()->host();
}

bool DOMWindow::isInsecureScriptAccess(DOMWindow& callingWindow, const String& urlString)
{
    if (!protocolIsJavaScript(urlString))
        return false;

    // If this DOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "callingWindow == this" check?
        if (&callingWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (callingWindow.frame()->securityContext()->securityOrigin()->canAccess(frame()->securityContext()->securityOrigin()))
            return false;
    }
    return true;
}

void DOMWindow::resetLocation()
{
    // Location needs to be reset manually because it doesn't inherit from DOMWindowProperty.
    // DOMWindowProperty is local-only, and Location needs to support remote windows, too.
    if (m_location) {
        m_location->reset();
        m_location = nullptr;
    }
}

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, LocalDOMWindow* source, ExceptionState& exceptionState)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* sourceDocument = source->document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SyntaxError exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return;
        target = sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique()) {
            exceptionState.throwDOMException(SyntaxError, "Invalid target origin '" + targetOrigin + "' in a call to 'postMessage'.");
            return;
        }
    }

    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    // FIXME: MixedContentChecker needs to be refactored for OOPIF.  For now,
    // create the url using replicated origins for remote frames.
    KURL targetUrl = isLocalDOMWindow() ? document()->url() : KURL(KURL(), frame()->securityContext()->securityOrigin()->toString());
    if (MixedContentChecker::isMixedContent(sourceDocument->securityOrigin(), targetUrl))
        UseCounter::count(frame(), UseCounter::PostMessageFromSecureToInsecure);
    else if (MixedContentChecker::isMixedContent(frame()->securityContext()->securityOrigin(), sourceDocument->url()))
        UseCounter::count(frame(), UseCounter::PostMessageFromInsecureToSecure);

    // Give the embedder a chance to intercept this postMessage.  If the
    // target is a remote frame, the message will be forwarded through the
    // browser process.
    RefPtrWillBeRawPtr<MessageEvent> event = MessageEvent::create(channels.release(), message, sourceOrigin, String(), source);
    bool didHandleMessageEvent = frame()->client()->willCheckAndDispatchMessageEvent(target.get(), event.get(), source->document()->frame());
    if (!didHandleMessageEvent) {
        // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
        RefPtrWillBeRawPtr<ScriptCallStack> stackTrace = nullptr;
        if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
            stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);

        toLocalDOMWindow(this)->schedulePostMessage(event, source, target.get(), stackTrace.release());
    }
}

DEFINE_TRACE(DOMWindow)
{
    visitor->trace(m_location);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
