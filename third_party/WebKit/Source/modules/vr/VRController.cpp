// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRController.h"

#include "core/frame/LocalFrame.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"

namespace blink {

VRController::~VRController()
{
}

void VRController::provideTo(LocalFrame& frame, ServiceRegistry* registry)
{
    ASSERT(RuntimeEnabledFeatures::webVREnabled());
    Supplement<LocalFrame>::provideTo(frame, supplementName(), registry ? new VRController(frame, registry) : nullptr);
}

VRController* VRController::from(LocalFrame& frame)
{
    return static_cast<VRController*>(Supplement<LocalFrame>::from(frame, supplementName()));
}

VRController::VRController(LocalFrame& frame, ServiceRegistry* registry)
    : LocalFrameLifecycleObserver(&frame)
{
    ASSERT(!m_service.is_bound());
    registry->connectToRemoteService(mojo::GetProxy(&m_service));
}

const char* VRController::supplementName()
{
    return "VRController";
}

void VRController::getDevices(std::unique_ptr<VRGetDevicesCallback> callback)
{
    if (!m_service) {
        callback->onError();
        return;
    }

    m_pendingGetDevicesCallbacks.append(std::move(callback));
    m_service->GetDevices(sameThreadBindForMojo(&VRController::OnGetDevices, this));
}

mojom::blink::VRSensorStatePtr VRController::getSensorState(unsigned index)
{
    if (!m_service)
        return nullptr;

    mojom::blink::VRSensorStatePtr state;
    m_service->GetSensorState(index, &state);
    return state;
}

void VRController::resetSensor(unsigned index)
{
    if (!m_service)
        return;
    m_service->ResetSensor(index);
}

void VRController::willDetachFrameHost()
{
    // TODO(kphanee): Detach from the mojo service connection.
}

void VRController::OnGetDevices(mojo::WTFArray<mojom::blink::VRDeviceInfoPtr> devices)
{
    std::unique_ptr<VRGetDevicesCallback> callback = m_pendingGetDevicesCallbacks.takeFirst();
    if (!callback)
        return;

    callback->onSuccess(std::move(devices));
}

DEFINE_TRACE(VRController)
{
    Supplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
