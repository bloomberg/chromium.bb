// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "core/frame/Navigator.h"
#include "modules/serviceworkers/NavigatorServiceWorkerInterface.h"

namespace WebCore {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator* navigator)
    : DOMWindowProperty(navigator->frame())
{
}

NavigatorServiceWorker::~NavigatorServiceWorker()
{
}

NavigatorServiceWorker* NavigatorServiceWorker::from(Navigator* navigator)
{
    NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
    if (!supplement) {
        supplement = new NavigatorServiceWorker(navigator);
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

const char* NavigatorServiceWorker::supplementName()
{
    return "NavigatorServiceWorker";
}

NavigatorServiceWorkerInterface* NavigatorServiceWorker::serviceWorker(Navigator* navigator)
{
    return NavigatorServiceWorker::from(navigator)->serviceWorker();
}

NavigatorServiceWorkerInterface* NavigatorServiceWorker::serviceWorker()
{
    if (!m_serviceWorker && frame())
        m_serviceWorker = NavigatorServiceWorkerInterface::create();
    return m_serviceWorker.get();
}

} // namespace WebCore
