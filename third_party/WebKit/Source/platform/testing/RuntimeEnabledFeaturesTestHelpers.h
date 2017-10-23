// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RuntimeEnabledFeaturesTestHelpers_h
#define RuntimeEnabledFeaturesTestHelpers_h

#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Assertions.h"

namespace blink {

template <bool (&getter)(), void (&setter)(bool)>
class ScopedRuntimeEnabledFeatureForTest {
 public:
  ScopedRuntimeEnabledFeatureForTest(bool enabled)
      : enabled_(enabled), original_(getter()) {
    setter(enabled);
  }

  ~ScopedRuntimeEnabledFeatureForTest() {
    CHECK_EQ(enabled_, getter());
    setter(original_);
  }

 private:
  bool enabled_;
  bool original_;
};

typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositeOpaqueFixedPositionEnabled,
    RuntimeEnabledFeatures::SetCompositeOpaqueFixedPositionEnabled>
    ScopedCompositeFixedPositionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled,
    RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled>
    ScopedCompositeOpaqueScrollersForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::AnimationWorkletEnabled,
    RuntimeEnabledFeatures::SetAnimationWorkletEnabled>
    ScopedAnimationWorkleForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::RootLayerScrollingEnabled,
    RuntimeEnabledFeatures::SetRootLayerScrollingEnabled>
    ScopedRootLayerScrollingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SlimmingPaintV175Enabled,
    RuntimeEnabledFeatures::SetSlimmingPaintV175Enabled>
    ScopedSlimmingPaintV175ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SlimmingPaintV2Enabled,
    RuntimeEnabledFeatures::SetSlimmingPaintV2Enabled>
    ScopedSlimmingPaintV2ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled,
    RuntimeEnabledFeatures::SetPaintUnderInvalidationCheckingEnabled>
    ScopedPaintUnderInvalidationCheckingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::AccessibilityObjectModelEnabled,
    RuntimeEnabledFeatures::SetAccessibilityObjectModelEnabled>
    ScopedAccessibilityObjectModelForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::MojoBlobsEnabled,
    RuntimeEnabledFeatures::SetMojoBlobsEnabled>
    ScopedMojoBlobsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::OverlayScrollbarsEnabled,
    RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled>
    ScopedOverlayScrollbarsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::ExperimentalCanvasFeaturesEnabled,
    RuntimeEnabledFeatures::SetExperimentalCanvasFeaturesEnabled>
    ScopedExperimentalCanvasFeaturesForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CSSVariables2Enabled,
    RuntimeEnabledFeatures::SetCSSVariables2Enabled>
    ScopedCSSVariables2ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled,
    RuntimeEnabledFeatures::SetCSSAdditiveAnimationsEnabled>
    ScopedCSSAdditiveAnimationsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::StackedCSSPropertyAnimationsEnabled,
    RuntimeEnabledFeatures::SetStackedCSSPropertyAnimationsEnabled>
    ScopedStackedCSSPropertyAnimationsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled,
    RuntimeEnabledFeatures::SetCSSPaintAPIArgumentsEnabled>
    ScopedCSSPaintAPIArgumentsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SuboriginsEnabled,
    RuntimeEnabledFeatures::SetSuboriginsEnabled>
    ScopedSuboriginsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled,
    RuntimeEnabledFeatures::SetCompositedSelectionUpdateEnabled>
    ScopedCompositedSelectionUpdateForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::ForceOverlayFullscreenVideoEnabled,
    RuntimeEnabledFeatures::SetForceOverlayFullscreenVideoEnabled>
    ScopedForceOverlayFullscreenVideoForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::OrientationEventEnabled,
    RuntimeEnabledFeatures::SetOrientationEventEnabled>
    ScopedOrientationEventForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled,
    RuntimeEnabledFeatures::SetMiddleClickAutoscrollEnabled>
    ScopedMiddleClickAutoscrollForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled,
    RuntimeEnabledFeatures::SetInputMultipleFieldsUIEnabled>
    ScopedInputMultipleFieldsUIForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CorsRFC1918Enabled,
    RuntimeEnabledFeatures::SetCorsRFC1918Enabled>
    ScopedCorsRFC1918ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::FeaturePolicyEnabled,
    RuntimeEnabledFeatures::SetFeaturePolicyEnabled>
    ScopedFeaturePolicyForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled,
    RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled>
    ScopedFractionalScrollOffsetsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::ScrollAnchoringEnabled,
    RuntimeEnabledFeatures::SetScrollAnchoringEnabled>
    ScopedScrollAnchoringForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SetRootScrollerEnabled,
    RuntimeEnabledFeatures::SetSetRootScrollerEnabled>
    ScopedRootScrollerForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::VideoFullscreenDetectionEnabled,
    RuntimeEnabledFeatures::SetVideoFullscreenDetectionEnabled>
    ScopedVideoFullscreenDetectionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::TrackLayoutPassesPerBlockEnabled,
    RuntimeEnabledFeatures::SetTrackLayoutPassesPerBlockEnabled>
    ScopedTrackLayoutPassesPerBlockForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::LayoutNGEnabled,
    RuntimeEnabledFeatures::SetLayoutNGEnabled>
    ScopedLayoutNGForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled,
    RuntimeEnabledFeatures::SetLayoutNGPaintFragmentsEnabled>
    ScopedLayoutNGPaintFragmentsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::ClientPlaceholdersForServerLoFiEnabled,
    RuntimeEnabledFeatures::SetClientPlaceholdersForServerLoFiEnabled>
    ScopedClientPlaceholdersForServerLoFiForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::OriginTrialsEnabled,
    RuntimeEnabledFeatures::SetOriginTrialsEnabled>
    ScopedOriginTrialsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled,
    RuntimeEnabledFeatures::SetTimerThrottlingForBackgroundTabsEnabled>
    ScopedTimerThrottlingForBackgroundTabsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::Canvas2dFixedRenderingModeEnabled,
    RuntimeEnabledFeatures::SetCanvas2dFixedRenderingModeEnabled>
    ScopedCanvas2dFixedRenderingModeForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::MediaCastOverlayButtonEnabled,
    RuntimeEnabledFeatures::SetMediaCastOverlayButtonEnabled>
    ScopedMediaCastOverlayButtonForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled,
    RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled>
    ScopedVideoRotateToFullscreenForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled,
    RuntimeEnabledFeatures::SetVideoFullscreenOrientationLockEnabled>
    ScopedVideoFullscreenOrientationLockForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::RemotePlaybackBackendEnabled,
    RuntimeEnabledFeatures::SetRemotePlaybackBackendEnabled>
    ScopedRemotePlaybackBackendForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled,
    RuntimeEnabledFeatures::SetNewRemotePlaybackPipelineEnabled>
    ScopedNewRemotePlaybackPipelineForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::FramesTimingFunctionEnabled,
    RuntimeEnabledFeatures::SetFramesTimingFunctionEnabled>
    ScopedFramesTimingFunctionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::BlinkRuntimeCallStatsEnabled,
    RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled>
    ScopedBlinkRuntimeCallStatsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositorImageAnimationsEnabled,
    RuntimeEnabledFeatures::SetCompositorImageAnimationsEnabled>
    ScopedCompositorImageAnimationsForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled,
    RuntimeEnabledFeatures::SetCanvas2dImageChromiumEnabled>
    ScopedCanvas2dImageChromiumForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::WebGLImageChromiumEnabled,
    RuntimeEnabledFeatures::SetWebGLImageChromiumEnabled>
    ScopedWebGLImageChromiumForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled,
    RuntimeEnabledFeatures::SetSignatureBasedIntegrityEnabled>
    ScopedSignatureBasedIntegrityForTest;
}  // namespace blink

#endif  // RuntimeEnabledFeaturesTestHelpers_h
