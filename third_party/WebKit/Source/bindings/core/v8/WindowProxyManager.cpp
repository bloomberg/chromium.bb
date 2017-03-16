// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/WindowProxyManager.h"

#include "bindings/core/v8/DOMWrapperWorld.h"

namespace blink {

DEFINE_TRACE(WindowProxyManager) {
  visitor->trace(m_frame);
  visitor->trace(m_windowProxy);
  visitor->trace(m_isolatedWorlds);
}

void WindowProxyManager::clearForClose() {
  m_windowProxy->clearForClose();
  for (auto& entry : m_isolatedWorlds)
    entry.value->clearForClose();
}

void WindowProxyManager::clearForNavigation() {
  m_windowProxy->clearForNavigation();
  for (auto& entry : m_isolatedWorlds)
    entry.value->clearForNavigation();
}

void WindowProxyManager::releaseGlobals(GlobalsVector& globals) {
  globals.reserveInitialCapacity(1 + m_isolatedWorlds.size());
  globals.emplace_back(&m_windowProxy->world(), m_windowProxy->releaseGlobal());
  for (auto& entry : m_isolatedWorlds) {
    globals.emplace_back(
        &entry.value->world(),
        windowProxyMaybeUninitialized(entry.value->world())->releaseGlobal());
  }
}

void WindowProxyManager::setGlobals(const GlobalsVector& globals) {
  for (const auto& entry : globals)
    windowProxyMaybeUninitialized(*entry.first)->setGlobal(entry.second);
}

WindowProxyManager::WindowProxyManager(Frame& frame, FrameType frameType)
    : m_isolate(v8::Isolate::GetCurrent()),
      m_frame(&frame),
      m_frameType(frameType),
      m_windowProxy(createWindowProxy(DOMWrapperWorld::mainWorld())) {}

WindowProxy* WindowProxyManager::createWindowProxy(DOMWrapperWorld& world) {
  switch (m_frameType) {
    case FrameType::Local:
      // Directly use static_cast instead of toLocalFrame because
      // WindowProxyManager gets instantiated during a construction of
      // LocalFrame and at that time virtual member functions are not yet
      // available (we cannot use LocalFrame::isLocalFrame).  Ditto for
      // RemoteFrame.
      return LocalWindowProxy::create(
          m_isolate, *static_cast<LocalFrame*>(m_frame.get()), &world);
    case FrameType::Remote:
      return RemoteWindowProxy::create(
          m_isolate, *static_cast<RemoteFrame*>(m_frame.get()), &world);
  }
  NOTREACHED();
  return nullptr;
}

WindowProxy* WindowProxyManager::windowProxyMaybeUninitialized(
    DOMWrapperWorld& world) {
  WindowProxy* windowProxy = nullptr;
  if (world.isMainWorld()) {
    windowProxy = m_windowProxy.get();
  } else {
    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world.worldId());
    if (iter != m_isolatedWorlds.end()) {
      windowProxy = iter->value.get();
    } else {
      windowProxy = createWindowProxy(world);
      m_isolatedWorlds.set(world.worldId(), windowProxy);
    }
  }
  return windowProxy;
}

void LocalWindowProxyManager::updateSecurityOrigin(
    SecurityOrigin* securityOrigin) {
  static_cast<LocalWindowProxy*>(m_windowProxy.get())
      ->updateSecurityOrigin(securityOrigin);

  for (auto& entry : m_isolatedWorlds) {
    auto* isolatedWindowProxy =
        static_cast<LocalWindowProxy*>(entry.value.get());
    SecurityOrigin* isolatedSecurityOrigin =
        isolatedWindowProxy->world().isolatedWorldSecurityOrigin();
    isolatedWindowProxy->updateSecurityOrigin(isolatedSecurityOrigin);
  }
}

}  // namespace blink
