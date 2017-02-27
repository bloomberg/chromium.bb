// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowProxyManager_h
#define WindowProxyManager_h

#include "bindings/core/v8/LocalWindowProxy.h"
#include "bindings/core/v8/RemoteWindowProxy.h"
#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "platform/heap/Handle.h"
#include <utility>
#include <v8.h>

namespace blink {

class DOMWrapperWorld;
class SecurityOrigin;
class ScriptController;

class WindowProxyManagerBase : public GarbageCollected<WindowProxyManagerBase> {
 public:
  DECLARE_TRACE();

  v8::Isolate* isolate() const { return m_isolate; }

  void clearForClose();
  void CORE_EXPORT clearForNavigation();

  void CORE_EXPORT
  releaseGlobals(HashMap<DOMWrapperWorld*, v8::Local<v8::Object>>&);
  void CORE_EXPORT
  setGlobals(const HashMap<DOMWrapperWorld*, v8::Local<v8::Object>>&);

 protected:
  using IsolatedWorldMap = HeapHashMap<int, Member<WindowProxy>>;

  explicit WindowProxyManagerBase(Frame&);

  Frame* frame() const { return m_frame; }
  WindowProxy* mainWorldProxy() const { return m_windowProxy.get(); }
  WindowProxy* windowProxy(DOMWrapperWorld&);

  IsolatedWorldMap& isolatedWorlds() { return m_isolatedWorlds; }

 private:
  v8::Isolate* const m_isolate;
  const Member<Frame> m_frame;
  const Member<WindowProxy> m_windowProxy;
  IsolatedWorldMap m_isolatedWorlds;
};

template <typename FrameType, typename ProxyType>
class WindowProxyManagerImplHelper : public WindowProxyManagerBase {
 private:
  using Base = WindowProxyManagerBase;

 public:
  FrameType* frame() const { return static_cast<FrameType*>(Base::frame()); }
  ProxyType* mainWorldProxy() const {
    return static_cast<ProxyType*>(Base::mainWorldProxy());
  }
  ProxyType* windowProxy(DOMWrapperWorld& world) {
    return static_cast<ProxyType*>(Base::windowProxy(world));
  }

 protected:
  explicit WindowProxyManagerImplHelper(Frame& frame)
      : WindowProxyManagerBase(frame) {}
};

class LocalWindowProxyManager
    : public WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy> {
 public:
  static LocalWindowProxyManager* create(LocalFrame& frame) {
    return new LocalWindowProxyManager(frame);
  }

  // Sets the given security origin to the main world's context.  Also updates
  // the security origin of the context for each isolated world.
  void updateSecurityOrigin(SecurityOrigin*);

 private:
  // TODO(dcheng): Merge LocalWindowProxyManager and ScriptController?
  friend class ScriptController;

  explicit LocalWindowProxyManager(LocalFrame& frame)
      : WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy>(frame) {}
};

class RemoteWindowProxyManager
    : public WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy> {
 public:
  static RemoteWindowProxyManager* create(RemoteFrame& frame) {
    return new RemoteWindowProxyManager(frame);
  }

 private:
  explicit RemoteWindowProxyManager(RemoteFrame& frame)
      : WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy>(frame) {}
};

}  // namespace blink

#endif  // WindowProxyManager_h
