// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/WindowProxyManager.h"

#include "bindings/core/v8/DOMWrapperWorld.h"

namespace blink {

namespace {

WindowProxy* createWindowProxyForFrame(v8::Isolate* isolate,
                                       Frame& frame,

                                       RefPtr<DOMWrapperWorld> world) {
  if (frame.isLocalFrame()) {
    return LocalWindowProxy::create(isolate, toLocalFrame(frame),
                                    std::move(world));
  }
  return RemoteWindowProxy::create(isolate, toRemoteFrame(frame),
                                   std::move(world));
}
}

DEFINE_TRACE(WindowProxyManagerBase) {
  visitor->trace(m_frame);
  visitor->trace(m_windowProxy);
  visitor->trace(m_isolatedWorlds);
}

WindowProxy* WindowProxyManagerBase::windowProxy(DOMWrapperWorld& world) {
  WindowProxy* windowProxy = nullptr;
  if (world.isMainWorld()) {
    windowProxy = m_windowProxy.get();
  } else {
    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world.worldId());
    if (iter != m_isolatedWorlds.end()) {
      windowProxy = iter->value.get();
    } else {
      windowProxy = createWindowProxyForFrame(m_isolate, *m_frame, &world);
      m_isolatedWorlds.set(world.worldId(), windowProxy);
    }
  }
  return windowProxy;
}

void WindowProxyManagerBase::clearForClose() {
  m_windowProxy->clearForClose();
  for (auto& entry : m_isolatedWorlds)
    entry.value->clearForClose();
}

void WindowProxyManagerBase::clearForNavigation() {
  m_windowProxy->clearForNavigation();
  for (auto& entry : m_isolatedWorlds)
    entry.value->clearForNavigation();
}

void WindowProxyManagerBase::releaseGlobals(GlobalsVector& globals) {
  globals.reserveInitialCapacity(1 + m_isolatedWorlds.size());
  globals.emplace_back(&m_windowProxy->world(), m_windowProxy->releaseGlobal());
  for (auto& entry : m_isolatedWorlds) {
    globals.emplace_back(&entry.value->world(),
                         windowProxy(entry.value->world())->releaseGlobal());
  }
}

void WindowProxyManagerBase::setGlobals(const GlobalsVector& globals) {
  for (const auto& entry : globals)
    windowProxy(*entry.first)->setGlobal(entry.second);
}

WindowProxyManagerBase::WindowProxyManagerBase(Frame& frame)
    : m_isolate(v8::Isolate::GetCurrent()),
      m_frame(&frame),
      m_windowProxy(createWindowProxyForFrame(m_isolate,
                                              frame,
                                              &DOMWrapperWorld::mainWorld())) {}

void LocalWindowProxyManager::updateSecurityOrigin(
    SecurityOrigin* securityOrigin) {
  static_cast<LocalWindowProxy*>(mainWorldProxy())
      ->updateSecurityOrigin(securityOrigin);
  for (auto& entry : isolatedWorlds()) {
    auto* isolatedWindowProxy =
        static_cast<LocalWindowProxy*>(entry.value.get());
    SecurityOrigin* isolatedSecurityOrigin =
        isolatedWindowProxy->world().isolatedWorldSecurityOrigin();
    isolatedWindowProxy->updateSecurityOrigin(isolatedSecurityOrigin);
  }
}

}  // namespace blink
