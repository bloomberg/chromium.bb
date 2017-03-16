// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowProxyManager_h
#define WindowProxyManager_h

#include <utility>

#include "bindings/core/v8/LocalWindowProxy.h"
#include "bindings/core/v8/RemoteWindowProxy.h"
#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class SecurityOrigin;

class WindowProxyManager : public GarbageCollected<WindowProxyManager> {
 public:
  DECLARE_TRACE();

  v8::Isolate* isolate() const { return m_isolate; }

  void clearForClose();
  void CORE_EXPORT clearForNavigation();

  // Globals are passed in a vector to maintain their order: global object for
  // the main world is always first. This is needed to prevent bugs like
  // https://crbug.com/700077.
  using GlobalsVector =
      Vector<std::pair<DOMWrapperWorld*, v8::Local<v8::Object>>>;
  void CORE_EXPORT releaseGlobals(GlobalsVector&);
  void CORE_EXPORT setGlobals(const GlobalsVector&);

  WindowProxy* mainWorldProxy() {
    m_windowProxy->initializeIfNeeded();
    return m_windowProxy;
  }

  WindowProxy* windowProxy(DOMWrapperWorld& world) {
    WindowProxy* windowProxy = windowProxyMaybeUninitialized(world);
    windowProxy->initializeIfNeeded();
    return windowProxy;
  }

 protected:
  using IsolatedWorldMap = HeapHashMap<int, Member<WindowProxy>>;
  enum class FrameType { Local, Remote };

  WindowProxyManager(Frame&, FrameType);

 private:
  // Creates an uninitialized WindowProxy.
  WindowProxy* createWindowProxy(DOMWrapperWorld&);
  WindowProxy* windowProxyMaybeUninitialized(DOMWrapperWorld&);

  v8::Isolate* const m_isolate;
  const Member<Frame> m_frame;
  const FrameType m_frameType;

 protected:
  const Member<WindowProxy> m_windowProxy;
  IsolatedWorldMap m_isolatedWorlds;
};

template <typename FrameType, typename ProxyType>
class WindowProxyManagerImplHelper : public WindowProxyManager {
 private:
  using Base = WindowProxyManager;

 public:
  ProxyType* mainWorldProxy() {
    return static_cast<ProxyType*>(Base::mainWorldProxy());
  }
  ProxyType* windowProxy(DOMWrapperWorld& world) {
    return static_cast<ProxyType*>(Base::windowProxy(world));
  }

 protected:
  WindowProxyManagerImplHelper(Frame& frame, FrameType frameType)
      : WindowProxyManager(frame, frameType) {}
};

class LocalWindowProxyManager
    : public WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy> {
 public:
  static LocalWindowProxyManager* create(LocalFrame& frame) {
    return new LocalWindowProxyManager(frame);
  }

  // TODO(yukishiino): Remove this method.
  LocalWindowProxy* mainWorldProxyMaybeUninitialized() {
    return static_cast<LocalWindowProxy*>(m_windowProxy.get());
  }

  // Sets the given security origin to the main world's context.  Also updates
  // the security origin of the context for each isolated world.
  void updateSecurityOrigin(SecurityOrigin*);

 private:
  explicit LocalWindowProxyManager(LocalFrame& frame)
      : WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy>(
            frame,
            FrameType::Local) {}
};

class RemoteWindowProxyManager
    : public WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy> {
 public:
  static RemoteWindowProxyManager* create(RemoteFrame& frame) {
    return new RemoteWindowProxyManager(frame);
  }

 private:
  explicit RemoteWindowProxyManager(RemoteFrame& frame)
      : WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy>(
            frame,
            FrameType::Remote) {}
};

}  // namespace blink

#endif  // WindowProxyManager_h
