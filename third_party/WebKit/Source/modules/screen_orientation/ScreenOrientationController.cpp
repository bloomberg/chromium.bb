// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"

namespace WebCore {

ScreenOrientationController::~ScreenOrientationController()
{
    ScreenOrientationDispatcher::instance().removeController(this);
}

ScreenOrientationController& ScreenOrientationController::from(Document& document)
{
    ScreenOrientationController* controller = static_cast<ScreenOrientationController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new ScreenOrientationController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return *controller;
}

ScreenOrientationController::ScreenOrientationController(Document& document)
    : m_document(document)
    , m_orientation(blink::WebScreenOrientationPortraitPrimary)
{
    // FIXME: We should listen for screen orientation change events only when the page is visible.
    ScreenOrientationDispatcher::instance().addController(this);
}

void ScreenOrientationController::dispatchOrientationChangeEvent()
{
    if (m_document.domWindow()
        && !m_document.activeDOMObjectsAreSuspended()
        && !m_document.activeDOMObjectsAreStopped())
        m_document.domWindow()->screen().dispatchEvent(Event::create(EventTypeNames::orientationchange));
}

const char* ScreenOrientationController::supplementName()
{
    return "ScreenOrientationController";
}

void ScreenOrientationController::didChangeScreenOrientation(blink::WebScreenOrientationType orientation)
{
    if (orientation == m_orientation)
        return;

    m_orientation = orientation;
    dispatchOrientationChangeEvent();
}

} // namespace WebCore
