// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplay_h
#define VRDisplay_h

#include "bindings/core/v8/v8_frame_request_callback.h"
#include "core/dom/Document.h"
#include "core/dom/PausableObject.h"
#include "core/dom/events/EventTarget.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VRLayerInit.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class NavigatorVR;
class VRController;
class VREyeParameters;
class VRFrameData;
class VRStageParameters;

class PLATFORM_EXPORT GpuMemoryBufferImageCopy;

class WebGLRenderingContextBase;

enum VREye { kVREyeNone, kVREyeLeft, kVREyeRight };

class VRDisplay final : public EventTargetWithInlineData,
                        public ActiveScriptWrappable<VRDisplay>,
                        public PausableObject,
                        public device::mojom::blink::VRDisplayClient,
                        public device::mojom::blink::VRSubmitFrameClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VRDisplay);
  USING_PRE_FINALIZER(VRDisplay, Dispose);

 public:
  ~VRDisplay() override;

  unsigned displayId() const { return display_id_; }
  const String& displayName() const { return display_name_; }

  VRDisplayCapabilities* capabilities() const { return capabilities_; }
  VRStageParameters* stageParameters() const { return stage_parameters_; }

  bool isPresenting() const { return is_presenting_; }

  bool getFrameData(VRFrameData*);

  double depthNear() const { return depth_near_; }
  double depthFar() const { return depth_far_; }

  void setDepthNear(double value) { depth_near_ = value; }
  void setDepthFar(double value) { depth_far_ = value; }

  VREyeParameters* getEyeParameters(const String&);

  int requestAnimationFrame(V8FrameRequestCallback*);
  void cancelAnimationFrame(int id);

  ScriptPromise requestPresent(ScriptState*,
                               const HeapVector<VRLayerInit>& layers);
  ScriptPromise exitPresent(ScriptState*);

  HeapVector<VRLayerInit> getLayers();

  void submitFrame();

  Document* GetDocument();

  // EventTarget overrides:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // PausableObject:
  void Pause() override;
  void Unpause() override;

  void FocusChanged();

  void OnMagicWindowVSync(double timestamp);

  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 protected:
  friend class VRController;

  VRDisplay(NavigatorVR*,
            device::mojom::blink::VRMagicWindowProviderPtr,
            device::mojom::blink::VRDisplayHostPtr,
            device::mojom::blink::VRDisplayClientRequest);

  void Update(const device::mojom::blink::VRDisplayInfoPtr&);

  void UpdatePose();

  void BeginPresent();
  void ForceExitPresent();

  void UpdateLayerBounds();

  VRController* Controller();

 private:
  // Specifies how submitFrame should transport frame data for the presenting
  // VR device, set by ConfigurePresentationPathForDisplay().
  enum class FrameTransport {
    // Invalid default value. Must be changed to a valid choice before starting
    // presentation.
    kUninitialized,

    // Command buffer CHROMIUM_texture_mailbox. Used by the Android Surface
    // rendering path.
    kMailbox,

    // A TextureHandle as extracted from a GpuMemoryBufferHandle. Used with
    // DXGI texture handles for OpenVR on Windows.
    kTextureHandle,
  };

  // Some implementations need to synchronize submitting with the completion of
  // the previous frame, i.e. the Android surface path needs to wait to avoid
  // lost frames in the transfer surface and to avoid overstuffed buffers. The
  // strategy choice here indicates at which point in the submission process
  // it should wait. NO_WAIT means to skip this wait entirely. For example,
  // the OpenVR render pipeline doesn't overlap frames, so the previous
  // frame is already guaranteed complete.
  enum class WaitPrevStrategy {
    kUninitialized,
    kNoWait,
    kBeforeBitmap,
    kAfterBitmap,
  };

  bool ConfigurePresentationPathForDisplay();
  void WaitForPreviousTransfer();
  WTF::TimeDelta WaitForPreviousRenderToFinish();

  void OnPresentComplete(bool);

  void OnConnected();
  void OnDisconnected();

  void StopPresenting();

  void OnPresentChange();

  // VRSubmitFrameClient
  void OnSubmitFrameTransferred(bool success) override;
  void OnSubmitFrameRendered() override;

  // VRDisplayClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;
  void OnActivate(device::mojom::blink::VRDisplayEventReason,
                  OnActivateCallback on_handled) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  void OnPresentingVSync(
      device::mojom::blink::VRPosePtr,
      WTF::TimeDelta,
      int16_t frame_id,
      device::mojom::blink::VRPresentationProvider::VSyncStatus);
  void OnPresentationProviderConnectionError();

  void OnMagicWindowPose(device::mojom::blink::VRPosePtr);

  bool FocusedOrPresenting();

  ScriptedAnimationController& EnsureScriptedAnimationController(Document*);
  void ProcessScheduledAnimations(double timestamp);
  void ProcessScheduledWindowAnimations(double timestamp);

  // Request delivery of a VSync event for either magic window mode or
  // presenting mode as applicable. May be called more than once per frame, it
  // ensures that there's at most one VSync request active at a time.
  // Does nothing if the web application hasn't requested a rAF callback.
  void RequestVSync();

  Member<NavigatorVR> navigator_vr_;
  unsigned display_id_ = 0;
  String display_name_;
  bool is_connected_ = false;
  bool is_presenting_ = false;
  bool is_valid_device_for_presenting_ = true;
  Member<VRDisplayCapabilities> capabilities_;
  Member<VRStageParameters> stage_parameters_;
  Member<VREyeParameters> eye_parameters_left_;
  Member<VREyeParameters> eye_parameters_right_;
  device::mojom::blink::VRPosePtr frame_pose_;
  device::mojom::blink::VRPosePtr pending_pose_;

  std::unique_ptr<GpuMemoryBufferImageCopy> frame_copier_;

  // This frame ID is vr-specific and is used to track when frames arrive at the
  // VR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t vr_frame_id_ = -1;
  VRLayerInit layer_;
  double depth_near_ = 0.01;
  double depth_far_ = 10000.0;

  // Current dimensions of the WebVR source canvas. May be different from
  // the recommended renderWidth/Height if the client overrides dimensions.
  int source_width_ = 0;
  int source_height_ = 0;

  void Dispose();

  gpu::gles2::GLES2Interface* context_gl_ = nullptr;
  Member<WebGLRenderingContextBase> rendering_context_;

  // Used to keep the image alive until the next frame if using
  // waitForPreviousTransferToFinish.
  scoped_refptr<Image> previous_image_;

  FrameTransport frame_transport_method_ = FrameTransport::kUninitialized;
  WaitPrevStrategy wait_for_previous_render_ = WaitPrevStrategy::kUninitialized;

  TraceWrapperMember<ScriptedAnimationController>
      scripted_animation_controller_;
  bool pending_vrdisplay_raf_ = false;
  bool pending_presenting_vsync_ = false;
  bool pending_magic_window_vsync_ = false;
  int pending_magic_window_vsync_id_ = -1;
  base::OnceClosure magic_window_vsync_waiting_for_pose_;
  WTF::TimeTicks magic_window_pose_time_;
  bool in_animation_frame_ = false;
  bool did_submit_this_frame_ = false;
  bool display_blurred_ = false;
  bool pending_previous_frame_render_ = false;
  bool pending_submit_frame_ = false;
  bool pending_present_request_ = false;
  bool last_transfer_succeeded_ = false;

  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRDisplayHostPtr display_;

  bool present_image_needs_copy_ = false;

  mojo::Binding<device::mojom::blink::VRSubmitFrameClient>
      submit_frame_client_binding_;
  mojo::Binding<device::mojom::blink::VRDisplayClient> display_client_binding_;
  device::mojom::blink::VRPresentationProviderPtr vr_presentation_provider_;

  HeapDeque<Member<ScriptPromiseResolver>> pending_present_resolvers_;
};

using VRDisplayVector = HeapVector<Member<VRDisplay>>;

enum class PresentationResult {
  kRequested = 0,
  kSuccess = 1,
  kSuccessAlreadyPresenting = 2,
  kVRDisplayCannotPresent = 3,
  kPresentationNotSupportedByDisplay = 4,
  kVRDisplayNotFound = 5,
  kNotInitiatedByUserGesture = 6,
  kInvalidNumberOfLayers = 7,
  kInvalidLayerSource = 8,
  kLayerSourceMissingWebGLContext = 9,
  kInvalidLayerBounds = 10,
  kServiceInactive = 11,
  kRequestDenied = 12,
  kFullscreenNotEnabled = 13,
  kPresentationResultMax,  // Must be last member of enum.
};

void ReportPresentationResult(PresentationResult);

}  // namespace blink

#endif  // VRDisplay_h
