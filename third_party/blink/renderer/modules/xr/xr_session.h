// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"

namespace blink {

class Element;
class ExceptionState;
class ResizeObserver;
class ScriptPromiseResolver;
class V8XRFrameRequestCallback;
class XR;
class XRCanvasInputProvider;
class XRSpace;
class XRInputSourceEvent;
class XRPresentationContext;
class XRRay;
class XRReferenceSpaceOptions;
class XRRenderState;
class XRRenderStateInit;
class XRView;
class XRViewerSpace;

class XRSession final : public EventTargetWithInlineData,
                        public device::mojom::blink::XRSessionClient,
                        public ActiveScriptWrappable<XRSession> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRSession);

 public:
  enum SessionMode {
    kModeInline = 0,
    kModeImmersiveVR,
    kModeImmersiveAR,
    kModeInlineAR
  };

  enum EnvironmentBlendMode {
    kBlendModeOpaque = 0,
    kBlendModeAdditive,
    kBlendModeAlphaBlend
  };

  // TODO(ddorwin): If https://github.com/immersive-web/webxr/issues/513 is
  // resolved in favor of removing `mode`, remove |mode_string|.
  XRSession(XR*,
            device::mojom::blink::XRSessionClientRequest client_request,
            SessionMode mode,
            const String& mode_string,
            EnvironmentBlendMode environment_blend_mode,
            bool sensorless_session);
  ~XRSession() override = default;

  XR* xr() const { return xr_; }
  const String& mode() const { return mode_string_; }
  bool environmentIntegration() const { return environment_integration_; }
  const String& environmentBlendMode() const { return blend_mode_string_; }
  XRRenderState* renderState() const { return render_state_; }
  XRSpace* viewerSpace() const;

  bool immersive() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blur, kBlur)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(focus, kFocus)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resetpose, kResetpose)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)

  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart, kSelectstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectend, kSelectend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(select, kSelect)

  void updateRenderState(XRRenderStateInit*, ExceptionState&);
  ScriptPromise requestReferenceSpace(ScriptState*,
                                      const XRReferenceSpaceOptions*);

  int requestAnimationFrame(V8XRFrameRequestCallback*);
  void cancelAnimationFrame(int id);

  using InputSourceMap = HeapHashMap<uint32_t, Member<XRInputSource>>;

  HeapVector<Member<XRInputSource>> getInputSources() const;

  ScriptPromise requestHitTest(ScriptState* script_state,
                               XRRay* ray,
                               XRSpace* space);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState*);

  bool ended() { return ended_; }

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  void ForceEnd();

  // Describes the scalar to be applied to the default framebuffer dimensions
  // which gives 1:1 pixel ratio at the center of the user's view.
  double NativeFramebufferScale() const;

  // Describes the recommended dimensions of layer framebuffers. Should be a
  // value that provides a good balance between quality and performance.
  DoubleSize DefaultFramebufferSize() const;

  // Reports the size of the output context's, if one is available. If not
  // reports (0, 0);
  DoubleSize OutputCanvasSize() const;
  XRPresentationContext* outputContext() const;
  void DetachOutputContext(XRPresentationContext* output_context);

  void LogGetPose() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocusChanged();
  void OnFrame(double timestamp,
               std::unique_ptr<TransformationMatrix>,
               const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
               const base::Optional<gpu::MailboxHolder>& bg_mailbox_holder,
               const base::Optional<IntSize>& background_size);
  void OnInputStateChange(
      int16_t frame_id,
      const WTF::Vector<device::mojom::blink::XRInputSourceStatePtr>&);

  const HeapVector<Member<XRView>>& views();

  void OnSelectStart(XRInputSource*);
  void OnSelectEnd(XRInputSource*);
  void OnSelect(XRInputSource*);

  void OnPoseReset();

  const device::mojom::blink::VRDisplayInfoPtr& GetVRDisplayInfo() const {
    return display_info_;
  }

  // TODO(jacde): Update the mojom to deliver this per-frame.
  bool EmulatedPosition() const {
    if (display_info_) {
      return !display_info_->capabilities->hasPosition;
    }

    // If we don't have display info then we should be using the identity
    // reference space, which by definition will be emulating the position.
    return true;
  }

  void UpdateEyeParameters(
      const device::mojom::blink::VREyeParametersPtr& left_eye,
      const device::mojom::blink::VREyeParametersPtr& right_eye);
  void UpdateStageParameters(
      const device::mojom::blink::VRStageParametersPtr& stage_parameters);
  bool External() const { return is_external_; }
  // Incremented every time display_info_ is changed, so that other objects that
  // depend on it can know when they need to update.
  unsigned int DisplayInfoPtrId() const { return display_info_id_; }

  void SetNonImmersiveProjectionMatrix(const WTF::Vector<float>&);
  void SetXRDisplayInfo(device::mojom::blink::VRDisplayInfoPtr display_info);

  void Trace(blink::Visitor*) override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

 private:
  class XRSessionResizeObserverDelegate;

  XRFrame* CreatePresentationFrame();
  void UpdateCanvasDimensions(Element*);
  void ApplyPendingRenderState();

  void UpdateInputSourceState(
      XRInputSource*,
      const device::mojom::blink::XRInputSourceStatePtr&);
  XRInputSourceEvent* CreateInputSourceEvent(const AtomicString&,
                                             XRInputSource*);

  // XRSessionClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnFocus() override;
  void OnBlur() override;

  bool HasAppropriateFocus();

  void OnHitTestResults(
      ScriptPromiseResolver* resolver,
      base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>>
          results);

  void EnsureEnvironmentErrorHandler();
  void OnEnvironmentProviderError();

  const Member<XR> xr_;
  const SessionMode mode_;
  const String mode_string_;
  const bool environment_integration_;
  String blend_mode_string_;
  Member<XRRenderState> render_state_;
  Member<XRViewerSpace> viewer_space_;
  HeapVector<Member<XRRenderStateInit>> pending_render_state_;
  HeapVector<Member<XRView>> views_;
  InputSourceMap input_sources_;
  Member<ResizeObserver> resize_observer_;
  Member<XRCanvasInputProvider> canvas_input_provider_;
  bool environment_error_handler_subscribed_ = false;
  HeapHashSet<Member<ScriptPromiseResolver>> hit_test_promises_;

  bool has_xr_focus_ = true;
  bool is_external_ = false;
  int display_info_id_ = 0;
  device::mojom::blink::VRDisplayInfoPtr display_info_;

  mojo::Binding<device::mojom::blink::XRSessionClient> client_binding_;

  Member<XRFrameRequestCallbackCollection> callback_collection_;
  std::unique_ptr<TransformationMatrix> base_pose_matrix_;

  WTF::Vector<float> non_immersive_projection_matrix_;

  bool blurred_;
  bool ended_ = false;
  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool update_views_next_frame_ = false;
  bool views_dirty_ = true;

  // Indicates that we've already logged a metric, so don't need to log it
  // again.
  mutable bool did_log_getInputSources_ = false;
  mutable bool did_log_getViewerPose_ = false;

  // Dimensions of the output canvas.
  int output_width_ = 1;
  int output_height_ = 1;

  // Indicates that this is a sensorless session which should only support the
  // identity reference space.
  bool sensorless_session_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
