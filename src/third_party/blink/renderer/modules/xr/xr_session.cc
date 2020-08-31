// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/inspector/console_message.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_hit_test_options_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_render_state_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_transient_input_hit_test_options_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_world_tracking_state_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_anchor_set.h"
#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_dom_overlay_state.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_ray.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_world_information.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kReferenceSpaceNotSupported[] =
    "This device does not support the requested reference space type.";

const char kIncompatibleLayer[] =
    "XRWebGLLayer was created with a different session.";

const char kInlineVerticalFOVNotSupported[] =
    "This session does not support inlineVerticalFieldOfView";

const char kAnchorsNotSupported[] = "Device does not support anchors!";

const char kDeviceDisconnected[] = "The XR device has been disconnected.";

const char kUnableToDecomposeMatrix[] =
    "The operation was unable to decompose a matrix and could not be "
    "completed.";

const char kUnableToRetrieveNativeOrigin[] =
    "The operation was unable to retrieve the native origin from XRSpace and "
    "could not be completed.";

const char kHitTestFeatureNotSupported[] =
    "Hit test feature is not supported by the session.";

const char kHitTestSubscriptionFailed[] = "Hit test subscription failed.";

const char kLightEstimationFeatureNotSupported[] =
    "Light estimation feature is not supported.";

const char kEntityTypesNotSpecified[] =
    "No entityTypes specified: the array cannot be empty!";

const double kDegToRad = M_PI / 180.0;

// Indices into the views array.
const unsigned int kMonoOrStereoLeftView = 0;
const unsigned int kStereoRightView = 1;

void UpdateViewFromEyeParameters(
    XRViewData& view,
    const device::mojom::blink::VREyeParametersPtr& eye,
    double depth_near,
    double depth_far) {
  const device::mojom::blink::VRFieldOfViewPtr& fov = eye->field_of_view;

  view.UpdateProjectionMatrixFromFoV(
      fov->up_degrees * kDegToRad, fov->down_degrees * kDegToRad,
      fov->left_degrees * kDegToRad, fov->right_degrees * kDegToRad, depth_near,
      depth_far);

  const TransformationMatrix matrix(eye->head_from_eye.matrix());
  view.SetHeadFromEyeTransform(matrix);
}

// Returns the session feature corresponding to the given reference space type.
base::Optional<device::mojom::XRSessionFeature> MapReferenceSpaceTypeToFeature(
    XRReferenceSpace::Type type) {
  switch (type) {
    case XRReferenceSpace::Type::kTypeViewer:
      return device::mojom::XRSessionFeature::REF_SPACE_VIEWER;
    case XRReferenceSpace::Type::kTypeLocal:
      return device::mojom::XRSessionFeature::REF_SPACE_LOCAL;
    case XRReferenceSpace::Type::kTypeLocalFloor:
      return device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR;
    case XRReferenceSpace::Type::kTypeBoundedFloor:
      return device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR;
    case XRReferenceSpace::Type::kTypeUnbounded:
      return device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED;
  }

  NOTREACHED();
  return base::nullopt;
}

std::unique_ptr<TransformationMatrix> getPoseMatrix(
    const device::mojom::blink::VRPosePtr& pose) {
  if (!pose)
    return nullptr;

  return std::make_unique<TransformationMatrix>(
      mojo::TypeConverter<TransformationMatrix,
                          device::mojom::blink::VRPosePtr>::Convert(pose));
}

base::Optional<device::mojom::blink::EntityTypeForHitTest>
EntityTypeForHitTestFromString(const String& string) {
  if (string == "plane")
    return device::mojom::blink::EntityTypeForHitTest::PLANE;

  if (string == "point")
    return device::mojom::blink::EntityTypeForHitTest::POINT;

  NOTREACHED();
  return base::nullopt;
}

// Returns a vector of entity types from hit test options, without duplicates.
// OptionsType can be either XRHitTestOptionsInit or
// XRTransientInputHitTestOptionsInit.
template <typename OptionsType>
Vector<device::mojom::blink::EntityTypeForHitTest> GetEntityTypesForHitTest(
    OptionsType* options_init) {
  DCHECK(options_init);
  HashSet<device::mojom::blink::EntityTypeForHitTest> result_set;

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes()) {
    DVLOG(2) << __func__ << ": options_init->entityTypes().size()="
             << options_init->entityTypes().size();
    for (const auto& entity_type_string : options_init->entityTypes()) {
      auto maybe_entity_type =
          EntityTypeForHitTestFromString(entity_type_string);

      if (maybe_entity_type) {
        result_set.insert(*maybe_entity_type);
      } else {
        DVLOG(1) << __func__
                 << ": entityTypes entry ignored:" << entity_type_string;
      }
    }
  } else {
    result_set.insert(device::mojom::blink::EntityTypeForHitTest::PLANE);
  }

  DVLOG(2) << __func__ << ": result_set.size()=" << result_set.size();
  DCHECK(!result_set.IsEmpty());

  Vector<device::mojom::blink::EntityTypeForHitTest> result;
  CopyToVector(result_set, result);

  DVLOG(2) << __func__ << ": result.size()=" << result.size();
  return result;
}

// Helper that will remove all entries present in the |id_to_hit_test_source|
// that map to nullptr due to usage of WeakPtr.
// T can be either XRHitTestSource or XRTransientInputHitTestSource.
template <typename T>
void CleanUpUnusedHitTestSourcesHelper(
    HeapHashMap<uint64_t, WeakMember<T>>* id_to_hit_test_source) {
  DCHECK(id_to_hit_test_source);

  // Gather all IDs of unused hit test sources for non-transient input
  // sources.
  HashSet<uint64_t> unused_hit_test_source_ids;
  for (auto& id_and_hit_test_source : *id_to_hit_test_source) {
    if (!id_and_hit_test_source.value) {
      unused_hit_test_source_ids.insert(id_and_hit_test_source.key);
    }
  }

  // Remove all of the unused hit test sources.
  id_to_hit_test_source->RemoveAll(unused_hit_test_source_ids);
}

// Helper that will validate that the passed in |hit_test_source| exists in
// |id_to_hit_test_source| map. The entry can be present but map to nullptr due
// to usage of WeakPtr - in that case, the entry will be removed.
// T can be either XRHitTestSource or XRTransientInputHitTestSource.
template <typename T>
bool ValidateHitTestSourceExistsHelper(
    HeapHashMap<uint64_t, WeakMember<T>>* id_to_hit_test_source,
    T* hit_test_source) {
  DCHECK(id_to_hit_test_source);
  DCHECK(hit_test_source);

  auto it = id_to_hit_test_source->find(hit_test_source->id());
  if (it == id_to_hit_test_source->end()) {
    return false;
  }

  if (!it->value) {
    id_to_hit_test_source->erase(it);
    return false;
  }

  return true;
}

}  // namespace

constexpr char XRSession::kNoRigidTransformSpecified[];
constexpr char XRSession::kUnableToRetrieveMatrix[];
constexpr char XRSession::kNoSpaceSpecified[];
constexpr char XRSession::kAnchorsFeatureNotSupported[];

class XRSession::XRSessionResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit XRSessionResizeObserverDelegate(XRSession* session)
      : session_(session) {
    DCHECK(session);
  }
  ~XRSessionResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    session_->UpdateCanvasDimensions(entries[0]->target());
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::MetricsReporter::MetricsReporter(
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder)
    : recorder_(std::move(recorder)) {}

void XRSession::MetricsReporter::ReportFeatureUsed(
    device::mojom::blink::XRSessionFeature feature) {
  using device::mojom::blink::XRSessionFeature;

  // If we've already reported using this feature, no need to report again.
  if (!reported_features_.insert(feature).is_new_entry) {
    return;
  }

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_VIEWER);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_LOCAL);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_LOCAL_FLOOR);
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_BOUNDED_FLOOR);
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_UNBOUNDED);
      break;
    case XRSessionFeature::DOM_OVERLAY:
    case XRSessionFeature::HIT_TEST:
    case XRSessionFeature::LIGHT_ESTIMATION:
    case XRSessionFeature::ANCHORS:
      // Not recording metrics for these features currently.
      break;
  }
}

XRSession::XRSession(
    XRSystem* xr,
    mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
        client_receiver,
    device::mojom::blink::XRSessionMode mode,
    EnvironmentBlendMode environment_blend_mode,
    InteractionMode interaction_mode,
    bool uses_input_eventing,
    bool sensorless_session,
    XRSessionFeatureSet enabled_features)
    : xr_(xr),
      mode_(mode),
      environment_integration_(
          mode == device::mojom::blink::XRSessionMode::kImmersiveAr),
      world_tracking_state_(MakeGarbageCollected<XRWorldTrackingState>()),
      world_information_(MakeGarbageCollected<XRWorldInformation>(this)),
      enabled_features_(std::move(enabled_features)),
      input_sources_(MakeGarbageCollected<XRInputSourceArray>()),
      client_receiver_(this, xr->GetExecutionContext()),
      input_receiver_(this, xr->GetExecutionContext()),
      callback_collection_(
          MakeGarbageCollected<XRFrameRequestCallbackCollection>(
              xr->GetExecutionContext())),
      uses_input_eventing_(uses_input_eventing),
      sensorless_session_(sensorless_session) {
  client_receiver_.Bind(
      std::move(client_receiver),
      xr->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  render_state_ = MakeGarbageCollected<XRRenderState>(immersive());
  // Ensure that frame focus is considered in the initial visibilityState.
  UpdateVisibilityState();

  switch (environment_blend_mode) {
    case kBlendModeOpaque:
      blend_mode_string_ = "opaque";
      break;
    case kBlendModeAdditive:
      blend_mode_string_ = "additive";
      break;
    case kBlendModeAlphaBlend:
      blend_mode_string_ = "alpha-blend";
      break;
    default:
      NOTREACHED() << "Unknown environment blend mode: "
                   << environment_blend_mode;
  }

  switch (interaction_mode) {
    case kInteractionModeScreen:
      interaction_mode_string_ = "screen-space";
      break;
    case kInteractionModeWorld:
      interaction_mode_string_ = "world-space";
      break;
  }
}

void XRSession::SetDOMOverlayElement(Element* element) {
  DVLOG(2) << __func__ << ": element=" << element;
  DCHECK(
      enabled_features_.Contains(device::mojom::XRSessionFeature::DOM_OVERLAY));
  DCHECK(element);

  overlay_element_ = element;

  // Set up the domOverlayState attribute. This could be done lazily on first
  // access, but it's a tiny object and it's unclear if the memory that might
  // save during XR sessions is worth the code size increase to do so. This
  // should be revisited if the state gets more complex in the future.
  //
  // At this time, "screen" is the only supported DOM Overlay type.
  dom_overlay_state_ = MakeGarbageCollected<XRDOMOverlayState>(
      XRDOMOverlayState::DOMOverlayType::kScreen);
}

const String XRSession::visibilityState() const {
  switch (visibility_state_) {
    case XRVisibilityState::VISIBLE:
      return "visible";
    case XRVisibilityState::VISIBLE_BLURRED:
      return "visible-blurred";
    case XRVisibilityState::HIDDEN:
      return "hidden";
  }
}

XRAnchorSet* XRSession::TrackedAnchors() const {
  DVLOG(3) << __func__;

  HeapHashSet<Member<XRAnchor>> result;

  if (is_tracked_anchors_null_)
    return nullptr;

  for (auto& anchor_id_and_anchor : anchor_ids_to_anchors_) {
    result.insert(anchor_id_and_anchor.value);
  }

  return MakeGarbageCollected<XRAnchorSet>(result);
}

bool XRSession::immersive() const {
  return mode_ == device::mojom::blink::XRSessionMode::kImmersiveVr ||
         mode_ == device::mojom::blink::XRSessionMode::kImmersiveAr;
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return event_target_names::kXRSession;
}

mojo::PendingAssociatedRemote<device::mojom::blink::XRInputSourceButtonListener>
XRSession::GetInputClickListener() {
  DCHECK(!input_receiver_.is_bound());
  return input_receiver_.BindNewEndpointAndPassRemote(
      xr_->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void XRSession::updateRenderState(XRRenderStateInit* init,
                                  ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  if (immersive() && init->hasInlineVerticalFieldOfView()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInlineVerticalFOVNotSupported);
    return;
  }

  // Validate that any baseLayer provided was created with this session.
  if (init->hasBaseLayer() && init->baseLayer() &&
      init->baseLayer()->session() != this) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kIncompatibleLayer);
    return;
  }

  pending_render_state_.push_back(init);

  // Updating our render state may have caused us to be in a state where we
  // should be requesting frames again. Kick off a new frame request in case
  // there are any pending callbacks to flush them out.
  MaybeRequestFrame();
}

void XRSession::updateWorldTrackingState(
    XRWorldTrackingStateInit* world_tracking_state_init,
    ExceptionState& exception_state) {
  DVLOG(3) << __func__;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  world_tracking_state_ =
      MakeGarbageCollected<XRWorldTrackingState>(world_tracking_state_init);
}

void XRSession::UpdateEyeParameters(
    const device::mojom::blink::VREyeParametersPtr& left_eye,
    const device::mojom::blink::VREyeParametersPtr& right_eye) {
  auto display_info = display_info_.Clone();
  display_info->left_eye = left_eye.Clone();
  display_info->right_eye = right_eye.Clone();
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::UpdateStageParameters(
    const device::mojom::blink::VRStageParametersPtr& stage_parameters) {
  auto display_info = display_info_.Clone();
  display_info->stage_parameters = stage_parameters.Clone();
  SetXRDisplayInfo(std::move(display_info));
}

ScriptPromise XRSession::requestReferenceSpace(
    ScriptState* script_state,
    const String& type,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  XRReferenceSpace::Type requested_type =
      XRReferenceSpace::StringToReferenceSpaceType(type);

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ReferenceSpace.Requested",
                            requested_type);

  if (sensorless_session_ &&
      requested_type != XRReferenceSpace::Type::kTypeViewer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  // If the session feature required by this reference space type is not
  // enabled, reject the session.
  auto type_as_feature = MapReferenceSpaceTypeToFeature(requested_type);
  if (!type_as_feature) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  // Report attempt to use this feature
  if (metrics_reporter_) {
    metrics_reporter_->ReportFeatureUsed(type_as_feature.value());
  }

  if (!IsFeatureEnabled(type_as_feature.value())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  XRReferenceSpace* reference_space = nullptr;
  switch (requested_type) {
    case XRReferenceSpace::Type::kTypeViewer:
    case XRReferenceSpace::Type::kTypeLocal:
    case XRReferenceSpace::Type::kTypeLocalFloor:
      reference_space =
          MakeGarbageCollected<XRReferenceSpace>(this, requested_type);
      break;
    case XRReferenceSpace::Type::kTypeBoundedFloor: {
      if (immersive()) {
        reference_space = MakeGarbageCollected<XRBoundedReferenceSpace>(this);
      }
      break;
    }
    case XRReferenceSpace::Type::kTypeUnbounded:
      if (immersive()) {
        reference_space = MakeGarbageCollected<XRReferenceSpace>(
            this, XRReferenceSpace::Type::kTypeUnbounded);
      }
      break;
  }

  // If the above switch statement failed to assign to reference_space,
  // it's because the reference space wasn't supported by the device.
  if (!reference_space) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  DCHECK(reference_space);
  reference_spaces_.push_back(reference_space);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(reference_space);

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ReferenceSpace.Succeeded",
                            requested_type);

  return promise;
}

ScriptPromise XRSession::CreateAnchorHelper(
    ScriptState* script_state,
    const blink::TransformationMatrix& native_origin_from_anchor,
    const XRNativeOriginInformation& native_origin_information,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  // Reject the promise if device doesn't support the anchors API.
  if (!xr_->xrEnvironmentProviderRemote()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kAnchorsNotSupported);
    return ScriptPromise();
  }

  TransformationMatrix::DecomposedType decomposed_native_origin_from_anchor;
  if (!native_origin_from_anchor.Decompose(
          decomposed_native_origin_from_anchor)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToDecomposeMatrix);
    return ScriptPromise();
  }

  // TODO(https://crbug.com/929841): Remove negation in quaternion once the bug
  // is fixed.
  device::mojom::blink::PosePtr pose_ptr = device::mojom::blink::Pose::New(
      gfx::Quaternion(-decomposed_native_origin_from_anchor.quaternion_x,
                      -decomposed_native_origin_from_anchor.quaternion_y,
                      -decomposed_native_origin_from_anchor.quaternion_z,
                      decomposed_native_origin_from_anchor.quaternion_w),
      blink::FloatPoint3D(decomposed_native_origin_from_anchor.translate_x,
                          decomposed_native_origin_from_anchor.translate_y,
                          decomposed_native_origin_from_anchor.translate_z));

  DVLOG(3) << __func__
           << ": pose_ptr->orientation = " << pose_ptr->orientation.ToString()
           << ", pose_ptr->position = " << pose_ptr->position.ToString();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->CreateAnchor(
      native_origin_information.ToMojo(), std::move(pose_ptr),
      WTF::Bind(&XRSession::OnCreateAnchorResult, WrapPersistent(this),
                WrapPersistent(resolver)));

  create_anchor_promises_.insert(resolver);

  return promise;
}

ScriptPromise XRSession::CreatePlaneAnchorHelper(
    ScriptState* script_state,
    const blink::TransformationMatrix& plane_from_anchor,
    uint64_t plane_id,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__ << ", plane_id=" << plane_id;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  // Reject the promise if device doesn't support the anchors API.
  if (!xr_->xrEnvironmentProviderRemote()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kAnchorsNotSupported);
    return ScriptPromise();
  }

  TransformationMatrix::DecomposedType decomposed_plane_from_anchor;
  if (!plane_from_anchor.Decompose(decomposed_plane_from_anchor)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToDecomposeMatrix);
    return ScriptPromise();
  }

  // TODO(https://crbug.com/929841): Remove negation in quaternion once the bug
  // is fixed.
  device::mojom::blink::PosePtr pose_ptr = device::mojom::blink::Pose::New(
      gfx::Quaternion(-decomposed_plane_from_anchor.quaternion_x,
                      -decomposed_plane_from_anchor.quaternion_y,
                      -decomposed_plane_from_anchor.quaternion_z,
                      decomposed_plane_from_anchor.quaternion_w),
      blink::FloatPoint3D(decomposed_plane_from_anchor.translate_x,
                          decomposed_plane_from_anchor.translate_y,
                          decomposed_plane_from_anchor.translate_z));

  DVLOG(3) << __func__
           << ": pose_ptr->orientation = " << pose_ptr->orientation.ToString()
           << ", pose_ptr->position = " << pose_ptr->position.ToString();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->CreatePlaneAnchor(
      std::move(pose_ptr), plane_id,
      WTF::Bind(&XRSession::OnCreateAnchorResult, WrapPersistent(this),
                WrapPersistent(resolver)));

  create_anchor_promises_.insert(resolver);

  return promise;
}

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  DVLOG(3) << __func__;

  TRACE_EVENT0("gpu", __func__);
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  int id = callback_collection_->RegisterCallback(callback);
  MaybeRequestFrame();
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_->CancelCallback(id);
}

XRInputSourceArray* XRSession::inputSources(ScriptState* script_state) const {
  if (!did_log_getInputSources_ && script_state->ContextIsValid()) {
    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidGetXRInputSources(1)
        .Record(LocalDOMWindow::From(script_state)->document()->UkmRecorder());
    did_log_getInputSources_ = true;
  }

  return input_sources_;
}

ScriptPromise XRSession::requestHitTestSource(
    ScriptState* script_state,
    XRHitTestOptionsInit* options_init,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK(options_init);

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::HIT_TEST)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kHitTestFeatureNotSupported);
    return {};
  }

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return {};
  }

  // 1. Grab the native origin from the passed in XRSpace.
  base::Optional<XRNativeOriginInformation> maybe_native_origin =
      options_init && options_init->hasSpace()
          ? options_init->space()->NativeOrigin()
          : base::nullopt;

  if (!maybe_native_origin) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToRetrieveNativeOrigin);
    return {};
  }

  // 2. Convert the XRRay to be expressed in terms of passed in XRSpace. This
  // should only matter for spaces whose transforms are not fully known on the
  // device (for example any space containing origin-offset).
  // Null checks not needed since native origin wouldn't be set if options_init
  // or space() were null.
  TransformationMatrix native_from_offset =
      options_init->space()->NativeFromOffsetMatrix();

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes() && options_init->entityTypes().IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kEntityTypesNotSpecified);
    return {};
  }

  auto entity_types = GetEntityTypesForHitTest(options_init);

  DVLOG(3) << __func__
           << ": native_from_offset = " << native_from_offset.ToString(true);

  // Transformation from passed in pose to |space|.

  XRRay* offsetRay = options_init && options_init->hasOffsetRay()
                         ? options_init->offsetRay()
                         : MakeGarbageCollected<XRRay>();
  auto space_from_ray = offsetRay->RawMatrix();
  auto origin_from_ray = native_from_offset * space_from_ray;

  DVLOG(3) << __func__
           << ": space_from_ray = " << space_from_ray.ToString(true);

  DVLOG(3) << __func__
           << ": origin_from_ray = " << origin_from_ray.ToString(true);

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();

  ray_mojo->origin = FloatPoint3D(origin_from_ray.MapPoint({0, 0, 0}));

  // Zero out the translation of origin_from_ray matrix to correctly map a 3D
  // vector.
  origin_from_ray.Translate3d(-origin_from_ray.M41(), -origin_from_ray.M42(),
                              -origin_from_ray.M43());

  auto direction = origin_from_ray.MapPoint({0, 0, -1});
  ray_mojo->direction = {direction.X(), direction.Y(), direction.Z()};

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->SubscribeToHitTest(
      maybe_native_origin->ToMojo(), entity_types, std::move(ray_mojo),
      WTF::Bind(&XRSession::OnSubscribeToHitTestResult, WrapPersistent(this),
                WrapPersistent(resolver)));
  request_hit_test_source_promises_.insert(resolver);

  return promise;
}

ScriptPromise XRSession::requestHitTestSourceForTransientInput(
    ScriptState* script_state,
    XRTransientInputHitTestOptionsInit* options_init,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK(options_init);

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::HIT_TEST)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kHitTestFeatureNotSupported);
    return {};
  }

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return {};
  }

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes() && options_init->entityTypes().IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kEntityTypesNotSpecified);
    return {};
  }

  auto entity_types = GetEntityTypesForHitTest(options_init);

  XRRay* offsetRay = options_init && options_init->hasOffsetRay()
                         ? options_init->offsetRay()
                         : MakeGarbageCollected<XRRay>();

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();
  ray_mojo->origin = {offsetRay->origin()->x(), offsetRay->origin()->y(),
                      offsetRay->origin()->z()};
  ray_mojo->direction = {offsetRay->direction()->x(),
                         offsetRay->direction()->y(),
                         offsetRay->direction()->z()};

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->SubscribeToHitTestForTransientInput(
      options_init->profile(), entity_types, std::move(ray_mojo),
      WTF::Bind(&XRSession::OnSubscribeToHitTestForTransientInputResult,
                WrapPersistent(this), WrapPersistent(resolver)));
  request_hit_test_source_promises_.insert(resolver);

  return promise;
}

void XRSession::OnSubscribeToHitTestResult(
    ScriptPromiseResolver* resolver,
    device::mojom::SubscribeToHitTestResult result,
    uint64_t subscription_id) {
  DVLOG(2) << __func__ << ": result=" << result
           << ", subscription_id=" << subscription_id;

  DCHECK(request_hit_test_source_promises_.Contains(resolver));
  request_hit_test_source_promises_.erase(resolver);

  if (result != device::mojom::SubscribeToHitTestResult::SUCCESS) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, kHitTestSubscriptionFailed));
    return;
  }

  XRHitTestSource* hit_test_source =
      MakeGarbageCollected<XRHitTestSource>(subscription_id, this);

  hit_test_source_ids_to_hit_test_sources_.insert(subscription_id,
                                                  hit_test_source);

  resolver->Resolve(hit_test_source);
}

void XRSession::OnSubscribeToHitTestForTransientInputResult(
    ScriptPromiseResolver* resolver,
    device::mojom::SubscribeToHitTestResult result,
    uint64_t subscription_id) {
  DVLOG(2) << __func__ << ": result=" << result
           << ", subscription_id=" << subscription_id;

  DCHECK(request_hit_test_source_promises_.Contains(resolver));
  request_hit_test_source_promises_.erase(resolver);

  if (result != device::mojom::SubscribeToHitTestResult::SUCCESS) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, kHitTestSubscriptionFailed));
    return;
  }

  XRTransientInputHitTestSource* hit_test_source =
      MakeGarbageCollected<XRTransientInputHitTestSource>(subscription_id,
                                                          this);

  hit_test_source_ids_to_transient_input_hit_test_sources_.insert(
      subscription_id, hit_test_source);

  resolver->Resolve(hit_test_source);
}

void XRSession::OnCreateAnchorResult(ScriptPromiseResolver* resolver,
                                     device::mojom::CreateAnchorResult result,
                                     uint64_t id) {
  DCHECK(create_anchor_promises_.Contains(resolver));
  create_anchor_promises_.erase(resolver);

  if (result == device::mojom::CreateAnchorResult::SUCCESS) {
    // Anchor was created successfully on the device. Subsequent frame update
    // must contain newly created anchor data.
    anchor_ids_to_pending_anchor_promises_.insert(id, resolver);
  } else {
    resolver->Reject();
  }
}

void XRSession::OnEnvironmentProviderCreated() {
  EnsureEnvironmentErrorHandler();
}

void XRSession::EnsureEnvironmentErrorHandler() {
  // Install error handler on environment provider to ensure that we get
  // notified so that we can clean up all relevant pending promises.
  if (!environment_error_handler_subscribed_ &&
      xr_->xrEnvironmentProviderRemote()) {
    environment_error_handler_subscribed_ = true;
    xr_->AddEnvironmentProviderErrorHandler(WTF::Bind(
        &XRSession::OnEnvironmentProviderError, WrapWeakPersistent(this)));
  }
}

void XRSession::OnEnvironmentProviderError() {
  HeapHashSet<Member<ScriptPromiseResolver>> create_anchor_promises;
  create_anchor_promises_.swap(create_anchor_promises);
  for (ScriptPromiseResolver* resolver : create_anchor_promises) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kDeviceDisconnected));
  }

  HeapHashSet<Member<ScriptPromiseResolver>> request_hit_test_source_promises;
  request_hit_test_source_promises_.swap(request_hit_test_source_promises);
  for (ScriptPromiseResolver* resolver : request_hit_test_source_promises) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kDeviceDisconnected));
  }
}

void XRSession::ProcessAnchorsData(
    const device::mojom::blink::XRAnchorsData* tracked_anchors_data,
    double timestamp) {
  TRACE_EVENT0("xr", __func__);

  if (!tracked_anchors_data) {
    DVLOG(3) << __func__ << ": tracked_anchors_data is null";

    // We have received a null ptr. Mark tracked_anchors as null & clear stored
    // anchors.
    is_tracked_anchors_null_ = true;
    anchor_ids_to_anchors_.clear();
    return;
  }

  TRACE_COUNTER2("xr", "Anchor statistics", "All anchors",
                 tracked_anchors_data->all_anchors_ids.size(),
                 "Updated anchors",
                 tracked_anchors_data->updated_anchors_data.size());

  DVLOG(3) << __func__ << ": updated anchors size="
           << tracked_anchors_data->updated_anchors_data.size()
           << ", all anchors size="
           << tracked_anchors_data->all_anchors_ids.size();

  is_tracked_anchors_null_ = false;

  HeapHashMap<uint64_t, Member<XRAnchor>> updated_anchors;

  // First, process all anchors that had their information updated (new anchors
  // are also processed here).
  for (const auto& anchor : tracked_anchors_data->updated_anchors_data) {
    DCHECK(anchor);

    auto it = anchor_ids_to_anchors_.find(anchor->id);
    if (it != anchor_ids_to_anchors_.end()) {
      updated_anchors.insert(anchor->id, it->value);
      it->value->Update(*anchor);
    } else {
      auto resolver_it =
          anchor_ids_to_pending_anchor_promises_.find(anchor->id);
      if (resolver_it == anchor_ids_to_pending_anchor_promises_.end()) {
        DCHECK(false)
            << "Newly created anchor must have a corresponding resolver!";
        continue;
      }

      XRAnchor* xr_anchor =
          MakeGarbageCollected<XRAnchor>(anchor->id, this, *anchor);
      resolver_it->value->Resolve(xr_anchor);
      anchor_ids_to_pending_anchor_promises_.erase(resolver_it);

      updated_anchors.insert(anchor->id, xr_anchor);
    }
  }

  // Then, copy over the anchors that were not updated but are still present.
  for (const auto& anchor_id : tracked_anchors_data->all_anchors_ids) {
    auto it_updated = updated_anchors.find(anchor_id);

    // If the anchor was already updated, there is nothing to do as it was
    // already moved to |updated_anchors|. Otherwise just copy it over as-is.
    if (it_updated == updated_anchors.end()) {
      auto it = anchor_ids_to_anchors_.find(anchor_id);
      DCHECK(it != anchor_ids_to_anchors_.end());
      updated_anchors.insert(anchor_id, it->value);
    }
  }

  DVLOG(3) << __func__
           << ": anchor count before update=" << anchor_ids_to_anchors_.size()
           << ", after update=" << updated_anchors.size();

  anchor_ids_to_anchors_.swap(updated_anchors);

  DCHECK(anchor_ids_to_pending_anchor_promises_.IsEmpty())
      << "All anchors should be updated in the frame in which they were "
         "created, got "
      << anchor_ids_to_pending_anchor_promises_.size()
      << " anchors that have not been updated";
}

void XRSession::CleanUpUnusedHitTestSources() {
  CleanUpUnusedHitTestSourcesHelper(&hit_test_source_ids_to_hit_test_sources_);

  CleanUpUnusedHitTestSourcesHelper(
      &hit_test_source_ids_to_transient_input_hit_test_sources_);

  DVLOG(3) << __func__ << ": Number of active hit test sources: "
           << hit_test_source_ids_to_hit_test_sources_.size()
           << ", number of active hit test sources for transient input: "
           << hit_test_source_ids_to_transient_input_hit_test_sources_.size();
}

void XRSession::ProcessHitTestData(
    const device::mojom::blink::XRHitTestSubscriptionResultsData*
        hit_test_subscriptions_data) {
  DVLOG(2) << __func__;

  CleanUpUnusedHitTestSources();

  if (hit_test_subscriptions_data) {
    // We have received hit test results for hit test subscriptions - process
    // each result and notify its corresponding hit test source about new
    // results for the current frame.
    DVLOG(3) << __func__ << "hit_test_subscriptions_data->results.size()="
             << hit_test_subscriptions_data->results.size() << ", "
             << "hit_test_subscriptions_data->transient_input_results.size()="
             << hit_test_subscriptions_data->transient_input_results.size();

    for (auto& hit_test_subscription_data :
         hit_test_subscriptions_data->results) {
      auto it = hit_test_source_ids_to_hit_test_sources_.find(
          hit_test_subscription_data->subscription_id);
      if (it != hit_test_source_ids_to_hit_test_sources_.end()) {
        it->value->Update(hit_test_subscription_data->hit_test_results);
      }
    }

    for (auto& transient_input_hit_test_subscription_data :
         hit_test_subscriptions_data->transient_input_results) {
      auto it = hit_test_source_ids_to_transient_input_hit_test_sources_.find(
          transient_input_hit_test_subscription_data->subscription_id);
      if (it !=
          hit_test_source_ids_to_transient_input_hit_test_sources_.end()) {
        it->value->Update(transient_input_hit_test_subscription_data
                              ->input_source_id_to_hit_test_results,
                          input_sources_);
      }
    }
  } else {
    DVLOG(3) << __func__ << ": hit_test_subscriptions_data unavailable";

    // We have not received hit test results for any of the hit test
    // subscriptions in the current frame - clean up the results on all hit test
    // source objects.
    for (auto& subscription_id_and_hit_test_source :
         hit_test_source_ids_to_hit_test_sources_) {
      subscription_id_and_hit_test_source.value->Update({});
    }

    for (auto& subscription_id_and_transient_input_hit_test_source :
         hit_test_source_ids_to_transient_input_hit_test_sources_) {
      subscription_id_and_transient_input_hit_test_source.value->Update(
          {}, nullptr);
    }
  }
}

ScriptPromise XRSession::requestLightProbe(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::LIGHT_ESTIMATION)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kLightEstimationFeatureNotSupported);
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!world_light_probe_) {
    world_light_probe_ = MakeGarbageCollected<XRLightProbe>(this);
  }

  resolver->Resolve(world_light_probe_);

  return promise;
}

ScriptPromise XRSession::end(ScriptState* script_state,
                             ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  // Don't allow a session to end twice.
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  ForceEnd(ShutdownPolicy::kWaitForResponse);

  end_session_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = end_session_resolver_->Promise();

  DVLOG(1) << __func__ << ": returning promise";
  return promise;
}

void XRSession::ForceEnd(ShutdownPolicy shutdown_policy) {
  bool wait_for_response;
  switch (shutdown_policy) {
    case ShutdownPolicy::kWaitForResponse:
      wait_for_response = true;
      break;
    case ShutdownPolicy::kImmediate:
      wait_for_response = false;
      break;
  }

  DVLOG(3) << __func__ << ": wait_for_response=" << wait_for_response
           << " ended_=" << ended_
           << " waiting_for_shutdown_=" << waiting_for_shutdown_;

  // If we've already ended, then just abort.  Since this is called only by C++
  // code, and predominantly just to ensure that the session is shut down, this
  // is fine.
  if (ended_) {
    // If we're currently waiting for an OnExitPresent, but are told not
    // to expect that anymore (i.e. due to a connection error), proceed
    // to full shutdown now.
    if (!wait_for_response && waiting_for_shutdown_) {
      HandleShutdown();
    }
    return;
  }

  // Detach this session from the XR system.
  ended_ = true;
  pending_frame_ = false;

  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    input_source->OnRemoved();
  }

  input_sources_ = nullptr;

  if (canvas_input_provider_) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }

  xr_->ExitPresent(
      WTF::Bind(&XRSession::OnExitPresent, WrapWeakPersistent(this)));

  if (wait_for_response) {
    waiting_for_shutdown_ = true;
  } else {
    HandleShutdown();
  }
}

void XRSession::HandleShutdown() {
  DVLOG(2) << __func__;
  DCHECK(ended_);
  waiting_for_shutdown_ = false;

  if (xr_->IsContextDestroyed()) {
    // If this is being called due to the context being destroyed,
    // it's illegal to run JavaScript code, so we cannot emit an
    // end event or resolve the stored promise. Don't bother calling
    // the frame provider's OnSessionEnded, that's being disposed of
    // also.
    DVLOG(3) << __func__ << ": Context destroyed";
    if (end_session_resolver_) {
      end_session_resolver_->Detach();
      end_session_resolver_ = nullptr;
    }
    return;
  }

  // Notify the frame provider that we've ended
  xr_->frameProvider()->OnSessionEnded(this);

  if (end_session_resolver_) {
    DVLOG(3) << __func__ << ": Resolving end_session_resolver_";
    end_session_resolver_->Resolve();
    end_session_resolver_ = nullptr;
  }

  DispatchEvent(*XRSessionEvent::Create(event_type_names::kEnd, this));
  DVLOG(3) << __func__ << ": session end event dispatched";
}

double XRSession::NativeFramebufferScale() const {
  if (immersive()) {
    double scale = display_info_->webxr_default_framebuffer_scale;
    DCHECK(scale);

    // Return the inverse of the default scale, since that's what we'll need to
    // multiply the default size by to get back to the native size.
    return 1.0 / scale;
  }
  return 1.0;
}

DoubleSize XRSession::DefaultFramebufferSize() const {
  if (!immersive()) {
    return OutputCanvasSize();
  }

  double scale = display_info_->webxr_default_framebuffer_scale;
  double width = display_info_->left_eye->render_width;
  double height = display_info_->left_eye->render_height;

  if (display_info_->right_eye) {
    width += display_info_->right_eye->render_width;
    height = std::max(display_info_->left_eye->render_height,
                      display_info_->right_eye->render_height);
  }

  return DoubleSize(width * scale, height * scale);
}

DoubleSize XRSession::OutputCanvasSize() const {
  if (!render_state_->output_canvas()) {
    return DoubleSize();
  }

  return DoubleSize(output_width_, output_height_);
}

void XRSession::OnFocusChanged() {
  UpdateVisibilityState();
}

void XRSession::OnVisibilityStateChanged(XRVisibilityState visibility_state) {
  // TODO(crbug.com/1002742): Until some ambiguities in the spec are cleared up,
  // force "visible-blurred" states from the device to report as "hidden"
  if (visibility_state == XRVisibilityState::VISIBLE_BLURRED) {
    visibility_state = XRVisibilityState::HIDDEN;
  }

  if (device_visibility_state_ != visibility_state) {
    device_visibility_state_ = visibility_state;
    UpdateVisibilityState();
  }
}

// The ultimate visibility state of the session is a combination of the devices
// reported visibility state and, for inline sessions, the frame focus, which
// will override the device visibility to "hidden" if the frame is not currently
// focused.
void XRSession::UpdateVisibilityState() {
  // Don't need to track the visibility state if the session has ended.
  if (ended_) {
    return;
  }

  XRVisibilityState state = device_visibility_state_;

  // The WebXR spec requires that if our document is not focused, that we don't
  // hand out real poses. For immersive sessions, we have to rely on the device
  // to tell us it's visibility state, as some runtimes (WMR) put focus in the
  // headset, and thus we cannot rely on Document Focus state. This is fine
  // because while the runtime reports us as focused the content owned by the
  // session should be focued, which is owned by the document. For inline, we
  // can and must rely on frame focus.
  if (!immersive() && !xr_->IsFrameFocused()) {
    state = XRVisibilityState::HIDDEN;
  }

  if (visibility_state_ != state) {
    visibility_state_ = state;

    // If the visibility state was changed to something other than hidden, we
    // may be able to restart the frame loop.
    MaybeRequestFrame();

    DispatchEvent(
        *XRSessionEvent::Create(event_type_names::kVisibilitychange, this));
  }
}

void XRSession::MaybeRequestFrame() {
  bool will_have_base_layer = !!render_state_->baseLayer();
  for (const auto& init : pending_render_state_) {
    if (init->hasBaseLayer()) {
      will_have_base_layer = !!init->baseLayer();
    }
  }

  // A page will not be allowed to get frames if its visibility state is hidden.
  bool page_allowed_frames = visibility_state_ != XRVisibilityState::HIDDEN;

  // A page is configured properly if it will have a base layer when the frame
  // callback gets resolved.
  bool page_configured_properly = will_have_base_layer;

  // If we have an outstanding callback registered, then we know that the page
  // actually wants frames.
  bool page_wants_frame = !callback_collection_->IsEmpty();

  // A page can process frames if it has its appropriate base layer set and has
  // indicated that it actually wants frames.
  bool page_can_process_frames = page_configured_properly && page_wants_frame;

  // We consider frames to be throttled if the page is not allowed frames, but
  // otherwise would be able to receive them. Therefore, if the page isn't in a
  // state to process frames, it doesn't matter if we are throttling it, any
  // "stalls" should be attributed to the page being poorly behaved.
  bool frames_throttled = page_can_process_frames && !page_allowed_frames;

  // If our throttled state has changed, notify anyone who may care
  if (frames_throttled_ != frames_throttled) {
    frames_throttled_ = frames_throttled;
    xr_->SetFramesThrottled(this, frames_throttled_);
  }

  // We can request a frame if we don't have one already pending, the page is
  // allowed to request frames, and the page is set up to properly handle frames
  // and wants one.
  bool request_frame =
      !pending_frame_ && page_allowed_frames && page_can_process_frames;
  if (request_frame) {
    xr_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
}

void XRSession::DetachOutputCanvas(HTMLCanvasElement* canvas) {
  if (!canvas)
    return;

  // Remove anything in this session observing the given output canvas.
  if (resize_observer_) {
    resize_observer_->unobserve(canvas);
  }

  if (canvas_input_provider_ && canvas_input_provider_->canvas() == canvas) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }
}

void XRSession::ApplyPendingRenderState() {
  DCHECK(!prev_base_layer_);
  if (pending_render_state_.size() > 0) {
    prev_base_layer_ = render_state_->baseLayer();
    HTMLCanvasElement* prev_ouput_canvas = render_state_->output_canvas();
    update_views_next_frame_ = true;

    // Loop through each pending render state and apply it to the active one.
    for (auto& init : pending_render_state_) {
      render_state_->Update(init);
    }
    pending_render_state_.clear();

    // If this is an inline session and the base layer has changed, give it an
    // opportunity to update it's drawing buffer size.
    if (!immersive() && render_state_->baseLayer() &&
        render_state_->baseLayer() != prev_base_layer_) {
      render_state_->baseLayer()->OnResize();
    }

    // If the output canvas changed, remove listeners from the old one and add
    // listeners to the new one as appropriate.
    if (prev_ouput_canvas != render_state_->output_canvas()) {
      // Remove anything observing the previous canvas.
      if (prev_ouput_canvas) {
        DetachOutputCanvas(prev_ouput_canvas);
      }

      // Monitor the new canvas for resize/input events.
      HTMLCanvasElement* canvas = render_state_->output_canvas();
      if (canvas) {
        if (!resize_observer_) {
          resize_observer_ = ResizeObserver::Create(
              canvas->GetDocument().domWindow(),
              MakeGarbageCollected<XRSessionResizeObserverDelegate>(this));
        }
        resize_observer_->observe(canvas);

        // Begin processing input events on the output context's canvas.
        if (!immersive()) {
          canvas_input_provider_ =
              MakeGarbageCollected<XRCanvasInputProvider>(this, canvas);
        }

        // Get the new canvas dimensions
        UpdateCanvasDimensions(canvas);
      }
    }
  }
}

void XRSession::UpdatePresentationFrameState(
    double timestamp,
    const device::mojom::blink::VRPosePtr& frame_pose,
    const device::mojom::blink::XRFrameDataPtr& frame_data,
    int16_t frame_id,
    bool emulated_position) {
  TRACE_EVENT0("gpu", __func__);
  DVLOG(2) << __func__ << " : frame_data valid? " << (frame_data ? true : false)
           << ", emulated_position=" << emulated_position;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  mojo_from_viewer_ = getPoseMatrix(frame_pose);
  DVLOG(2) << __func__ << " : mojo_from_viewer_ valid? "
           << (mojo_from_viewer_ ? true : false);

  emulated_position_ = emulated_position;

  // Process XR input sources
  if (frame_data) {
    base::span<const device::mojom::blink::XRInputSourceStatePtr> input_states;
    if (frame_data->input_state.has_value())
      input_states = frame_data->input_state.value();

    OnInputStateChangeInternal(frame_id, input_states);

    // World understanding includes hit testing for transient input sources, and
    // these sources may have been hidden when touching DOM Overlay content
    // that's inside cross-origin iframes. Since hit test subscriptions only
    // happen for existing input_sources_ entries, these touches will not
    // generate hit test results. For this to work, this step must happen
    // after OnInputStateChangeInternal which updated input sources.
    UpdateWorldUnderstandingStateForFrame(timestamp, frame_data);

    // If this session uses input eventing, XR select events are handled via
    // OnButtonEvent, so they need to be ignored here to avoid duplicate events.
    if (!uses_input_eventing_) {
      ProcessInputSourceEvents(input_states);
    }
  } else {
    UpdateWorldUnderstandingStateForFrame(timestamp, frame_data);
  }
}

void XRSession::UpdateWorldUnderstandingStateForFrame(
    double timestamp,
    const device::mojom::blink::XRFrameDataPtr& frame_data) {
  // Update objects that might change on per-frame basis.
  if (frame_data) {
    world_information_->ProcessPlaneInformation(
        frame_data->detected_planes_data.get(), timestamp);
    ProcessAnchorsData(frame_data->anchors_data.get(), timestamp);
    ProcessHitTestData(frame_data->hit_test_subscription_results.get());

    const device::mojom::blink::XRLightEstimationData* light_data =
        frame_data->light_estimation_data.get();
    if (world_light_probe_ && light_data) {
      world_light_probe_->ProcessLightEstimationData(light_data, timestamp);
    }
  } else {
    world_information_->ProcessPlaneInformation(nullptr, timestamp);
    ProcessAnchorsData(nullptr, timestamp);
    ProcessHitTestData(nullptr);

    if (world_light_probe_) {
      world_light_probe_->ProcessLightEstimationData(nullptr, timestamp);
    }
  }
}

bool XRSession::IsFeatureEnabled(
    device::mojom::XRSessionFeature feature) const {
  return enabled_features_.Contains(feature);
}

void XRSession::SetMetricsReporter(std::unique_ptr<MetricsReporter> reporter) {
  DCHECK(!metrics_reporter_);
  metrics_reporter_ = std::move(reporter);
}

void XRSession::OnFrame(
    double timestamp,
    const base::Optional<gpu::MailboxHolder>& output_mailbox_holder) {
  TRACE_EVENT0("gpu", __func__);
  DVLOG(2) << __func__ << ": ended_=" << ended_
           << ", pending_frame_=" << pending_frame_;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  // If there are pending render state changes, apply them now.
  prev_base_layer_ = nullptr;
  ApplyPendingRenderState();

  if (pending_frame_) {
    pending_frame_ = false;

    // Don't allow frames to be processed if there's no layers attached to the
    // session. That would allow tracking with no associated visuals.
    XRWebGLLayer* frame_base_layer = render_state_->baseLayer();
    if (!frame_base_layer) {
      DVLOG(2) << __func__ << ": frame_base_layer not present";

      // If we previously had a frame base layer, we need to still attempt to
      // submit a frame back to the runtime, as all "GetFrameData" calls need a
      // matching submit.
      if (prev_base_layer_) {
        DVLOG(2) << __func__
                 << ": prev_base_layer_ is valid, submitting frame to it";
        prev_base_layer_->OnFrameStart(output_mailbox_holder);
        prev_base_layer_->OnFrameEnd();
        prev_base_layer_ = nullptr;
      }
      return;
    }

    // Don't allow frames to be processed if an inline session doesn't have an
    // output canvas.
    if (!immersive() && !render_state_->output_canvas()) {
      DVLOG(2) << __func__
               << ": frames are not to be processed if an inline session "
                  "doesn't have an output canvas";
      return;
    }

    frame_base_layer->OnFrameStart(output_mailbox_holder);

    // Don't allow frames to be processed if the session's visibility state is
    // "hidden".
    if (visibility_state_ == XRVisibilityState::HIDDEN) {
      DVLOG(2) << __func__
               << ": frames to be processed if the session's visibility state "
                  "is \"hidden\"";
      // If the frame is skipped because of the visibility state,
      // make sure we end the frame anyway.
      frame_base_layer->OnFrameEnd();
      return;
    }

    XRFrame* presentation_frame = CreatePresentationFrame();
    presentation_frame->SetAnimationFrame(true);

    // Make sure that any frame-bounded changed to the views array take effect.
    if (update_views_next_frame_) {
      views_dirty_ = true;
      update_views_next_frame_ = false;
    }

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    base::AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_->ExecuteCallbacks(this, timestamp, presentation_frame);

    // The session might have ended in the middle of the frame. Only call
    // OnFrameEnd if it's still valid.
    if (!ended_)
      frame_base_layer->OnFrameEnd();

    // Ensure the XRFrame cannot be used outside the callbacks.
    presentation_frame->Deactivate();
  }
}

void XRSession::LogGetPose() const {
  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!did_log_getViewerPose_ && window) {
    did_log_getViewerPose_ = true;

    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidRequestPose(1)
        .Record(window->document()->UkmRecorder());
  }
}

bool XRSession::CanReportPoses() const {
  // The spec has a few requirements for if poses can be reported.
  // If we have a session, then user intent is understood. Therefore, (due to
  // the way visibility state is updatd), the rest of the steps really just
  // boil down to whether or not the XRVisibilityState is Visible.
  return visibility_state_ == XRVisibilityState::VISIBLE;
}

base::Optional<TransformationMatrix> XRSession::GetMojoFrom(
    XRReferenceSpace::Type space_type) {
  if (!CanReportPoses())
    return base::nullopt;

  switch (space_type) {
    case XRReferenceSpace::Type::kTypeViewer:
      if (!mojo_from_viewer_) {
        if (sensorless_session_) {
          return TransformationMatrix();
        }

        return base::nullopt;
      }

      return *mojo_from_viewer_;
    case XRReferenceSpace::Type::kTypeLocal:
      // TODO(https://crbug.com/1070380): This assumes that local space is
      // equivalent to mojo space! Remove the assumption once the bug is fixed.
      return TransformationMatrix();
    case XRReferenceSpace::Type::kTypeUnbounded:
      // TODO(https://crbug.com/1070380): This assumes that unbounded space is
      // equivalent to mojo space! Remove the assumption once the bug is fixed.
      return TransformationMatrix();
    case XRReferenceSpace::Type::kTypeLocalFloor:
    case XRReferenceSpace::Type::kTypeBoundedFloor:
      // Information about -floor spaces is currently stored elsewhere (in stage
      // parameters of display_info_). It probably should eventually move here.
      return base::nullopt;
  }
}

XRFrame* XRSession::CreatePresentationFrame() {
  DVLOG(2) << __func__;

  XRFrame* presentation_frame =
      MakeGarbageCollected<XRFrame>(this, world_information_);
  return presentation_frame;
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalDOMWindow* window = To<LocalDOMWindow>(xr_->GetExecutionContext());
  if (window) {
    devicePixelRatio = window->GetFrame()->DevicePixelRatio();
  }

  update_views_next_frame_ = true;
  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;
  int output_angle = 0;

  // TODO(crbug.com/836948): handle square canvases.
  // TODO(crbug.com/840346): we should not need to use ScreenOrientation here.
  ScreenOrientation* orientation = ScreenOrientation::Create(window);

  if (orientation) {
    output_angle = orientation->angle();
    DVLOG(2) << __func__ << ": got angle=" << output_angle;
  }

  if (render_state_->baseLayer()) {
    render_state_->baseLayer()->OnResize();
  }
}

void XRSession::OnButtonEvent(
    device::mojom::blink::XRInputSourceStatePtr input_state) {
  DCHECK(uses_input_eventing_);
  auto input_states = base::make_span(&input_state, 1);
  OnInputStateChangeInternal(last_frame_id_, input_states);
  ProcessInputSourceEvents(input_states);
}

void XRSession::OnInputStateChangeInternal(
    int16_t frame_id,
    base::span<const device::mojom::blink::XRInputSourceStatePtr>
        input_states) {
  // If we're in any state other than visible, input should not be processed
  if (visibility_state_ != XRVisibilityState::VISIBLE) {
    return;
  }

  HeapVector<Member<XRInputSource>> added;
  HeapVector<Member<XRInputSource>> removed;
  last_frame_id_ = frame_id;

  DVLOG(2) << __func__ << ": frame_id=" << frame_id
           << " input_states.size()=" << input_states.size();
  // Build up our added array, and update the frame id of any active input
  // sources so we can flag the ones that are no longer active.
  for (const auto& input_state : input_states) {
    DVLOG(2) << __func__
             << ": input_state->source_id=" << input_state->source_id
             << " input_state->primary_input_pressed="
             << input_state->primary_input_pressed
             << " clicked=" << input_state->primary_input_clicked;

    XRInputSource* stored_input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    DVLOG(2) << __func__ << ": stored_input_source=" << stored_input_source;
    XRInputSource* input_source = XRInputSource::CreateOrUpdateFrom(
        stored_input_source, this, input_state);

    // Input sources should use DOM overlay hit test to check if they intersect
    // cross-origin content. If that's the case, the input source is set as
    // invisible, and must not return poses or hit test results.
    bool hide_input_source = false;
    if (overlay_element_ && input_state->overlay_pointer_position) {
      input_source->ProcessOverlayHitTest(overlay_element_, input_state);
      if (!stored_input_source && !input_source->IsVisible()) {
        DVLOG(2) << __func__ << ": (new) hidden_input_source";
        hide_input_source = true;
      }
    }

    // Using pointer equality to determine if the pointer needs to be set.
    if (stored_input_source != input_source) {
      DVLOG(2) << __func__ << ": stored_input_source != input_source";
      if (!hide_input_source) {
        input_sources_->SetWithSourceId(input_state->source_id, input_source);
        added.push_back(input_source);
        DVLOG(2) << __func__ << ": ADDED input_source "
                 << input_state->source_id;
      }

      // If we previously had a stored_input_source, disconnect its gamepad
      // and mark that it was removed.
      if (stored_input_source) {
        stored_input_source->SetGamepadConnected(false);
        DVLOG(2) << __func__ << ": REMOVED stored_input_source";
        removed.push_back(stored_input_source);
      }
    }

    input_source->setActiveFrameId(frame_id);
  }

  // Remove any input sources that are inactive, and disconnect their gamepad.
  // Note that this is done in two passes because HeapHashMap makes no
  // guarantees about iterators on removal.
  // We use a separate array of inactive sources here rather than just
  // processing removed, because if we replaced any input sources, they would
  // also be in removed, and we'd remove our newly added source.
  Vector<uint32_t> inactive_sources;
  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    if (input_source->activeFrameId() != frame_id) {
      inactive_sources.push_back(input_source->source_id());
      input_source->OnRemoved();
      removed.push_back(input_source);
    }
  }

  for (uint32_t source_id : inactive_sources) {
    input_sources_->RemoveWithSourceId(source_id);
  }

  // If there have been any changes, fire the input sources change event.
  if (!added.IsEmpty() || !removed.IsEmpty()) {
    DispatchEvent(*XRInputSourcesChangeEvent::Create(
        event_type_names::kInputsourceschange, this, added, removed));
  }
}

void XRSession::ProcessInputSourceEvents(
    base::span<const device::mojom::blink::XRInputSourceStatePtr>
        input_states) {
  for (const auto& input_state : input_states) {
    // If anything during the process of updating the select state caused us
    // to end our session, we should stop processing select state updates.
    if (ended_)
      break;

    XRInputSource* input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    // The input source might not be in input_sources_ if it was created hidden.
    if (input_source) {
      input_source->UpdateButtonStates(input_state);
    }
  }
}

void XRSession::AddTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  // Ensure we're not overriding an input source that's already present.
  DCHECK(!input_sources_->GetWithSourceId(input_source->source_id()));
  input_sources_->SetWithSourceId(input_source->source_id(), input_source);

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {input_source}, {}));
}

void XRSession::RemoveTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  input_sources_->RemoveWithSourceId(input_source->source_id());

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {}, {input_source}));
}

void XRSession::OnMojoSpaceReset() {
  for (const auto& reference_space : reference_spaces_) {
    reference_space->OnReset();
  }
}

void XRSession::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::OnExitPresent() {
  DVLOG(2) << __func__ << ": immersive()=" << immersive()
           << " waiting_for_shutdown_=" << waiting_for_shutdown_;
  if (immersive()) {
    ForceEnd(ShutdownPolicy::kImmediate);
  } else if (waiting_for_shutdown_) {
    HandleShutdown();
  }
}

bool XRSession::ValidateHitTestSourceExists(XRHitTestSource* hit_test_source) {
  return ValidateHitTestSourceExistsHelper(
      &hit_test_source_ids_to_hit_test_sources_, hit_test_source);
}

bool XRSession::ValidateHitTestSourceExists(
    XRTransientInputHitTestSource* hit_test_source) {
  return ValidateHitTestSourceExistsHelper(
      &hit_test_source_ids_to_transient_input_hit_test_sources_,
      hit_test_source);
}

bool XRSession::RemoveHitTestSource(XRHitTestSource* hit_test_source) {
  DCHECK(hit_test_source);
  bool result = ValidateHitTestSourceExistsHelper(
      &hit_test_source_ids_to_hit_test_sources_, hit_test_source);

  hit_test_source_ids_to_hit_test_sources_.erase(hit_test_source->id());

  return result;
}

bool XRSession::RemoveHitTestSource(
    XRTransientInputHitTestSource* hit_test_source) {
  DCHECK(hit_test_source);
  bool result = ValidateHitTestSourceExistsHelper(
      &hit_test_source_ids_to_transient_input_hit_test_sources_,
      hit_test_source);

  hit_test_source_ids_to_transient_input_hit_test_sources_.erase(
      hit_test_source->id());

  return result;
}

void XRSession::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  // We don't necessarily trust the backend to only send us display info changes
  // when something has actually changed, and a change here can trigger several
  // other interfaces to recompute data or fire events, so it's worthwhile to
  // validate that an actual change has occurred.
  if (display_info_) {
    if (display_info_->Equals(*display_info))
      return;

    if (display_info_->stage_parameters && display_info->stage_parameters &&
        !display_info_->stage_parameters->Equals(
            *(display_info->stage_parameters))) {
      // Stage parameters changed.
      stage_parameters_id_++;
    } else if (!!(display_info_->stage_parameters) !=
               !!(display_info->stage_parameters)) {
      // Either stage parameters just became available (sometimes happens if
      // detecting the bounds doesn't happen until a few seconds into the
      // session for platforms such as WMR), or the stage parameters just went
      // away (probably due to tracking loss).
      stage_parameters_id_++;
    }
  } else if (display_info && display_info->stage_parameters) {
    // Got stage parameters for the first time this session.
    stage_parameters_id_++;
  }

  display_info_id_++;
  display_info_ = std::move(display_info);
}

Vector<XRViewData>& XRSession::views() {
  // TODO(bajones): For now we assume that immersive sessions render a stereo
  // pair of views and non-immersive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service.
  if (views_dirty_) {
    if (immersive()) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.emplace_back(XRView::kEyeLeft);
        if (display_info_->right_eye) {
          views_.emplace_back(XRView::kEyeRight);
        }
      }
      // In immersive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(
          views_[kMonoOrStereoLeftView], display_info_->left_eye,
          render_state_->depthNear(), render_state_->depthFar());
      if (display_info_->right_eye) {
        UpdateViewFromEyeParameters(
            views_[kStereoRightView], display_info_->right_eye,
            render_state_->depthNear(), render_state_->depthFar());
      }
    } else {
      if (views_.IsEmpty()) {
        views_.emplace_back(XRView::kEyeNone);
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      // In non-immersive mode, if there is no explicit projection matrix
      // provided, the projection matrix must be aligned with the
      // output canvas dimensions.
      base::Optional<double> inline_vertical_fov =
          render_state_->inlineVerticalFieldOfView();

      // inlineVerticalFieldOfView should only be null in immersive mode.
      DCHECK(inline_vertical_fov.has_value());
      views_[kMonoOrStereoLeftView].UpdateProjectionMatrixFromAspect(
          inline_vertical_fov.value(), aspect, render_state_->depthNear(),
          render_state_->depthFar());
    }

    views_dirty_ = false;
  }

  return views_;
}

bool XRSession::HasPendingActivity() const {
  return !callback_collection_->IsEmpty() && !ended_;
}

void XRSession::Trace(Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(render_state_);
  visitor->Trace(world_tracking_state_);
  visitor->Trace(world_information_);
  visitor->Trace(world_light_probe_);
  visitor->Trace(pending_render_state_);
  visitor->Trace(end_session_resolver_);
  visitor->Trace(input_sources_);
  visitor->Trace(resize_observer_);
  visitor->Trace(canvas_input_provider_);
  visitor->Trace(overlay_element_);
  visitor->Trace(dom_overlay_state_);
  visitor->Trace(client_receiver_);
  visitor->Trace(input_receiver_);
  visitor->Trace(callback_collection_);
  visitor->Trace(create_anchor_promises_);
  visitor->Trace(request_hit_test_source_promises_);
  visitor->Trace(reference_spaces_);
  visitor->Trace(anchor_ids_to_anchors_);
  visitor->Trace(anchor_ids_to_pending_anchor_promises_);
  visitor->Trace(prev_base_layer_);
  visitor->Trace(hit_test_source_ids_to_hit_test_sources_);
  visitor->Trace(hit_test_source_ids_to_transient_input_hit_test_sources_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
