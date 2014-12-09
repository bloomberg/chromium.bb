// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/DOMWindow.h"

#include "core/frame/Frame.h"
#include "core/frame/FrameClient.h"

namespace blink {

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

} // namespace blink
