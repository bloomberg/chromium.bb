// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "core/frame/Navigator.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

namespace WebCore {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator)
    : DOMWindowProperty(navigator.frame())
{
}

NavigatorServiceWorker::~NavigatorServiceWorker()
{
}

NavigatorServiceWorker& NavigatorServiceWorker::from(Navigator& navigator)
{
    NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
    if (!supplement) {
        supplement = new NavigatorServiceWorker(navigator);
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return *supplement;
}

const char* NavigatorServiceWorker::supplementName()
{
    return "NavigatorServiceWorker";
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(Navigator& navigator)
{
    return NavigatorServiceWorker::from(navigator).serviceWorker();
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker()
{
    if (!m_serviceWorker && frame())
        m_serviceWorker = ServiceWorkerContainer::create();
    return m_serviceWorker.get();
}

void NavigatorServiceWorker::willDetachGlobalObjectFromFrame()
{
    m_serviceWorker->detachClient();
    m_serviceWorker = nullptr;
}

} // namespace WebCore
