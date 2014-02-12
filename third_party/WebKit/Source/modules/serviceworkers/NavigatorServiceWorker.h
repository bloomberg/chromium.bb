// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorServiceWorker_h
#define NavigatorServiceWorker_h

#include "bindings/v8/ScriptPromise.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace WebCore {

class Navigator;
class NavigatorServiceWorkerInterface;

class NavigatorServiceWorker FINAL : public Supplement<Navigator>, DOMWindowProperty {
public:
    virtual ~NavigatorServiceWorker();
    static NavigatorServiceWorker* from(Navigator*);
    static NavigatorServiceWorker* toNavigatorServiceWorker(Navigator* navigator) { return static_cast<NavigatorServiceWorker*>(Supplement<Navigator>::from(navigator, supplementName())); }
    static const char* supplementName();

    static NavigatorServiceWorkerInterface* serviceWorker(Navigator*);

private:
    explicit NavigatorServiceWorker(Navigator*);
    NavigatorServiceWorkerInterface* serviceWorker();

    RefPtr<NavigatorServiceWorkerInterface> m_serviceWorker;
};

} // namespace WebCore

#endif // NavigatorServiceWorker_h
