// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplay_h
#define VRDisplay_h

#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/events/EventTarget.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VRLayer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class NavigatorVR;
class ScriptedAnimationController;
class VRController;
class VREyeParameters;
class VRFrameData;
class VRStageParameters;
class VRPose;

class WebGLRenderingContextBase;

enum VREye { VREyeNone, VREyeLeft, VREyeRight };

class VRDisplay final : public EventTargetWithInlineData,
                        public ActiveScriptWrappable<VRDisplay>,
                        public ContextLifecycleObserver,
                        public device::mojom::blink::VRDisplayClient,
                        public device::mojom::blink::VRSubmitFrameClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VRDisplay);
  USING_PRE_FINALIZER(VRDisplay, dispose);

 public:
  ~VRDisplay();

  unsigned displayId() const { return m_displayId; }
  const String& displayName() const { return m_displayName; }

  VRDisplayCapabilities* capabilities() const { return m_capabilities; }
  VRStageParameters* stageParameters() const { return m_stageParameters; }

  bool isConnected() const { return m_isConnected; }
  bool isPresenting() const { return m_isPresenting; }

  bool getFrameData(VRFrameData*);
  VRPose* getPose();
  void resetPose();

  double depthNear() const { return m_depthNear; }
  double depthFar() const { return m_depthFar; }

  void setDepthNear(double value) { m_depthNear = value; }
  void setDepthFar(double value) { m_depthFar = value; }

  VREyeParameters* getEyeParameters(const String&);

  int requestAnimationFrame(FrameRequestCallback*);
  void cancelAnimationFrame(int id);

  ScriptPromise requestPresent(ScriptState*, const HeapVector<VRLayer>& layers);
  ScriptPromise exitPresent(ScriptState*);

  HeapVector<VRLayer> getLayers();

  void submitFrame();

  Document* document();

  // EventTarget overrides:
  ExecutionContext* getExecutionContext() const override;
  const AtomicString& interfaceName() const override;

  // ContextLifecycleObserver implementation.
  void contextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  void focusChanged();

  DECLARE_VIRTUAL_TRACE();

 protected:
  friend class VRController;

  VRDisplay(NavigatorVR*,
            device::mojom::blink::VRDisplayPtr,
            device::mojom::blink::VRDisplayClientRequest);

  void update(const device::mojom::blink::VRDisplayInfoPtr&);

  void updatePose();

  void beginPresent();
  void forceExitPresent();

  void updateLayerBounds();
  void disconnected();

  VRController* controller();

 private:
  void onPresentComplete(bool);

  void onConnected();
  void onDisconnected();

  void stopPresenting();

  void OnPresentChange();

  // VRSubmitFrameClient
  void OnSubmitFrameTransferred();
  void OnSubmitFrameRendered();

  // VRDisplayClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;
  void OnActivate(device::mojom::blink::VRDisplayEventReason) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  void OnVSync(device::mojom::blink::VRPosePtr,
               mojo::common::mojom::blink::TimeDeltaPtr,
               int16_t frameId,
               device::mojom::blink::VRVSyncProvider::Status);
  void ConnectVSyncProvider();
  void OnVSyncConnectionError();

  ScriptedAnimationController& ensureScriptedAnimationController(Document*);
  void processScheduledAnimations(double timestamp);

  Member<NavigatorVR> m_navigatorVR;
  unsigned m_displayId = 0;
  String m_displayName;
  bool m_isConnected = false;
  bool m_isPresenting = false;
  bool m_isValidDeviceForPresenting = true;
  Member<VRDisplayCapabilities> m_capabilities;
  Member<VRStageParameters> m_stageParameters;
  Member<VREyeParameters> m_eyeParametersLeft;
  Member<VREyeParameters> m_eyeParametersRight;
  device::mojom::blink::VRPosePtr m_framePose;

  // This frame ID is vr-specific and is used to track when frames arrive at the
  // VR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t m_vrFrameId = -1;
  VRLayer m_layer;
  double m_depthNear = 0.01;
  double m_depthFar = 10000.0;

  // Current dimensions of the WebVR source canvas. May be different from
  // the recommended renderWidth/Height if the client overrides dimensions.
  int m_sourceWidth = 0;
  int m_sourceHeight = 0;

  void dispose();

  gpu::gles2::GLES2Interface* m_contextGL = nullptr;
  Member<WebGLRenderingContextBase> m_renderingContext;

  // Used to keep the image alive until the next frame if using
  // waitForPreviousTransferToFinish.
  RefPtr<Image> m_previousImage;

  Member<ScriptedAnimationController> m_scriptedAnimationController;
  bool m_pendingRaf = false;
  bool m_pendingVsync = false;
  bool m_inAnimationFrame = false;
  bool m_inDisplayActivate = false;
  bool m_displayBlurred = false;
  double m_timebase = -1;
  bool m_pendingPreviousFrameRender = false;
  bool m_pendingSubmitFrame = false;

  device::mojom::blink::VRDisplayPtr m_display;

  mojo::Binding<device::mojom::blink::VRSubmitFrameClient>
      m_submit_frame_client_binding;
  mojo::Binding<device::mojom::blink::VRDisplayClient> m_displayClientBinding;
  device::mojom::blink::VRVSyncProviderPtr m_vrVSyncProvider;

  HeapDeque<Member<ScriptPromiseResolver>> m_pendingPresentResolvers;
};

using VRDisplayVector = HeapVector<Member<VRDisplay>>;

enum class PresentationResult {
  Requested = 0,
  Success = 1,
  SuccessAlreadyPresenting = 2,
  VRDisplayCannotPresent = 3,
  PresentationNotSupportedByDisplay = 4,
  VRDisplayNotFound = 5,
  NotInitiatedByUserGesture = 6,
  InvalidNumberOfLayers = 7,
  InvalidLayerSource = 8,
  LayerSourceMissingWebGLContext = 9,
  InvalidLayerBounds = 10,
  ServiceInactive = 11,
  RequestDenied = 12,
  FullscreenNotEnabled = 13,
  PresentationResultMax,  // Must be last member of enum.
};

void ReportPresentationResult(PresentationResult);

}  // namespace blink

#endif  // VRDisplay_h
