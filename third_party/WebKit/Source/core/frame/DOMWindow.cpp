// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/DOMWindow.h"

#include "core/dom/SecurityContext.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameClient.h"
#include "core/frame/Location.h"
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

DEFINE_TRACE(DOMWindow)
{
    visitor->trace(m_location);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
