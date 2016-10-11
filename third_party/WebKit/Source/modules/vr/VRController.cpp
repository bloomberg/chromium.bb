// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRController.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "public/platform/InterfaceProvider.h"

#include "wtf/Assertions.h"

namespace blink {

VRController::VRController(NavigatorVR* navigatorVR)
    : ContextLifecycleObserver(navigatorVR->document()),
      m_navigatorVR(navigatorVR),
      m_binding(this) {
  navigatorVR->document()->frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_service));
  m_service->SetClient(m_binding.CreateInterfacePtrAndBind());
}

VRController::~VRController() {}

void VRController::getDisplays(ScriptPromiseResolver* resolver) {
  if (!m_service) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "The service is no longer active.");
    resolver->reject(exception);
    return;
  }

  m_pendingGetDevicesCallbacks.append(
      WTF::wrapUnique(new VRGetDevicesCallback(resolver)));
  m_service->GetDisplays(convertToBaseCallback(
      WTF::bind(&VRController::onGetDisplays, wrapPersistent(this))));
}

device::blink::VRPosePtr VRController::getPose(unsigned index) {
  if (!m_service)
    return nullptr;

  device::blink::VRPosePtr pose;
  m_service->GetPose(index, &pose);
  return pose;
}

void VRController::resetPose(unsigned index) {
  if (!m_service)
    return;

  m_service->ResetPose(index);
}

void VRController::requestPresent(ScriptPromiseResolver* resolver,
                                  unsigned index,
                                  bool secureOrigin) {
  if (!m_service) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "The service is no longer active.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::ServiceInactive);
    return;
  }

  m_service->RequestPresent(
      index, secureOrigin,
      convertToBaseCallback(WTF::bind(&VRController::onPresentComplete,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver), index)));
}

void VRController::exitPresent(unsigned index) {
  if (!m_service)
    return;

  m_service->ExitPresent(index);
}

void VRController::submitFrame(unsigned index, device::blink::VRPosePtr pose) {
  if (!m_service)
    return;

  m_service->SubmitFrame(index, std::move(pose));
}

void VRController::updateLayerBounds(
    unsigned index,
    device::blink::VRLayerBoundsPtr leftBounds,
    device::blink::VRLayerBoundsPtr rightBounds) {
  if (!m_service)
    return;

  m_service->UpdateLayerBounds(index, std::move(leftBounds),
                               std::move(rightBounds));
}

VRDisplay* VRController::createOrUpdateDisplay(
    const device::blink::VRDisplayPtr& display) {
  VRDisplay* vrDisplay = getDisplayForIndex(display->index);
  if (!vrDisplay) {
    vrDisplay = new VRDisplay(m_navigatorVR);
    m_displays.append(vrDisplay);
  }

  vrDisplay->update(display);
  return vrDisplay;
}

VRDisplayVector VRController::updateDisplays(
    mojo::WTFArray<device::blink::VRDisplayPtr> displays) {
  VRDisplayVector vrDisplays;

  for (const auto& display : displays.PassStorage()) {
    VRDisplay* vrDisplay = createOrUpdateDisplay(display);
    vrDisplays.append(vrDisplay);
  }

  return vrDisplays;
}

VRDisplay* VRController::getDisplayForIndex(unsigned index) {
  VRDisplay* display;
  for (size_t i = 0; i < m_displays.size(); ++i) {
    display = m_displays[i];
    if (display->displayId() == index) {
      return display;
    }
  }

  return 0;
}

void VRController::onGetDisplays(
    mojo::WTFArray<device::blink::VRDisplayPtr> displays) {
  VRDisplayVector outDisplays = updateDisplays(std::move(displays));

  std::unique_ptr<VRGetDevicesCallback> callback =
      m_pendingGetDevicesCallbacks.takeFirst();
  if (!callback)
    return;

  callback->onSuccess(outDisplays);
}

void VRController::onPresentComplete(ScriptPromiseResolver* resolver,
                                     unsigned index,
                                     bool success) {
  VRDisplay* vrDisplay = getDisplayForIndex(index);
  if (!vrDisplay) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRDisplay not found.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::VRDisplayNotFound);
    return;
  }

  if (success) {
    vrDisplay->beginPresent(resolver);
  } else {
    vrDisplay->forceExitPresent();
    DOMException* exception = DOMException::create(
        NotAllowedError, "Presentation request was denied.");
    ReportPresentationResult(PresentationResult::RequestDenied);
    resolver->reject(exception);
  }
}

void VRController::OnDisplayChanged(device::blink::VRDisplayPtr display) {
  VRDisplay* vrDisplay = getDisplayForIndex(display->index);
  if (!vrDisplay)
    return;

  vrDisplay->update(display);
}

void VRController::OnExitPresent(unsigned index) {
  VRDisplay* vrDisplay = getDisplayForIndex(index);
  if (vrDisplay)
    vrDisplay->forceExitPresent();
}

void VRController::OnDisplayConnected(device::blink::VRDisplayPtr display) {
  VRDisplay* vrDisplay = createOrUpdateDisplay(display);
  if (!vrDisplay)
    return;

  m_navigatorVR->fireVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayconnect, true, false, vrDisplay, "connect"));
}

void VRController::OnDisplayDisconnected(unsigned index) {
  VRDisplay* vrDisplay = getDisplayForIndex(index);
  if (!vrDisplay)
    return;

  vrDisplay->disconnected();

  m_navigatorVR->fireVREvent(
      VRDisplayEvent::create(EventTypeNames::vrdisplaydisconnect, true, false,
                             vrDisplay, "disconnect"));
}

void VRController::contextDestroyed() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  m_binding.Close();
  m_service.reset();

  // The context is not automatically cleared, so do it manually.
  ContextLifecycleObserver::clearContext();
}

DEFINE_TRACE(VRController) {
  visitor->trace(m_navigatorVR);
  visitor->trace(m_displays);

  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
