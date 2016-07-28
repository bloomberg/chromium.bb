// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRController_h
#define VRController_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplay.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"

#include <memory>

namespace blink {

class NavigatorVR;
class ServiceRegistry;
class VRGetDevicesCallback;

class VRController final
    : public GarbageCollectedFinalized<VRController>
    , public device::blink::VRServiceClient
    , public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(VRController);
    WTF_MAKE_NONCOPYABLE(VRController);
public:
    VRController(NavigatorVR*);
    virtual ~VRController();

    // VRService.
    void getDisplays(ScriptPromiseResolver*);
    device::blink::VRPosePtr getPose(unsigned index);
    void resetPose(unsigned index);

    VRDisplay* createOrUpdateDisplay(const device::blink::VRDisplayPtr&);
    VRDisplayVector updateDisplays(mojo::WTFArray<device::blink::VRDisplayPtr>);
    VRDisplay* getDisplayForIndex(unsigned index);

    DECLARE_VIRTUAL_TRACE();

private:
    // Binding callbacks.
    void onGetDisplays(mojo::WTFArray<device::blink::VRDisplayPtr>);

    // VRServiceClient.
    void OnDisplayChanged(device::blink::VRDisplayPtr) override;

    // ContextLifecycleObserver.
    void contextDestroyed() override;

    Member<NavigatorVR> m_navigatorVR;
    VRDisplayVector m_displays;

    Deque<std::unique_ptr<VRGetDevicesCallback>> m_pendingGetDevicesCallbacks;
    device::blink::VRServicePtr m_service;
    mojo::Binding<device::blink::VRServiceClient> m_binding;
};

} // namespace blink

#endif // VRController_h
