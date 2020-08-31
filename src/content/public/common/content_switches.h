// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/media_buildflags.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const char kAcceleratedCanvas2dMSAASampleCount[];
CONTENT_EXPORT extern const char kAllowPreCommitInput[];
CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowInsecureLocalhost[];
CONTENT_EXPORT extern const char kAllowLoopbackInPeerConnection[];
CONTENT_EXPORT extern const char kAllowPopupsDuringPageUnload[];
CONTENT_EXPORT extern const char kAllowSyncXHRInPageDismissal[];
CONTENT_EXPORT extern const char kAndroidFontsPath[];
CONTENT_EXPORT extern const char kAppCacheForceEnabled[];
CONTENT_EXPORT extern const char kBlinkSettings[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserStartupDialog[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
CONTENT_EXPORT extern const char kBrowserTest[];
CONTENT_EXPORT extern const char kConversionsDebugMode[];
CONTENT_EXPORT extern const char kDefaultTileWidth[];
CONTENT_EXPORT extern const char kDefaultTileHeight[];
CONTENT_EXPORT extern const char kMinHeightForGpuRasterTile[];
CONTENT_EXPORT extern const char kDisable2dCanvasAntialiasing[];
CONTENT_EXPORT extern const char kDisable2dCanvasImageChromium[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableYUVImageDecoding[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoEncode[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char
    kDisableBackgroundingOccludedWindowsForTesting[];
CONTENT_EXPORT extern const char kDisableBackgroundTimerThrottling[];
CONTENT_EXPORT extern const char kDisableBlinkFeatures[];
CONTENT_EXPORT extern const char kDisableDatabases[];
CONTENT_EXPORT extern const char kDisableDisplayList2dCanvas[];
extern const char kDisableDomainBlockingFor3DAPIs[];
CONTENT_EXPORT extern const char kDisableWebGL[];
CONTENT_EXPORT extern const char kDisableWebGL2[];
CONTENT_EXPORT extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableFlash3d[];
CONTENT_EXPORT extern const char kDisableFlashStage3d[];
CONTENT_EXPORT extern const char kDisableGestureRequirementForPresentation[];
CONTENT_EXPORT extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGpuCompositing[];
CONTENT_EXPORT extern const char kDisableGpuEarlyInit[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferCompositorResources[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferVideoFrames[];
extern const char kDisableGpuProcessCrashLimit[];
CONTENT_EXPORT extern const char kDisableSoftwareCompositingFallback[];
CONTENT_EXPORT extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableImageAnimationResync[];
CONTENT_EXPORT extern const char kDisableIpcFloodingProtection[];
CONTENT_EXPORT extern const char kDisableJavaScriptHarmonyShipping[];
CONTENT_EXPORT extern const char kDisableLowLatencyDxva[];
CONTENT_EXPORT extern const char kDisableLowResTiling[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableHistogramCustomizer[];
CONTENT_EXPORT extern const char kDisableLCDText[];
CONTENT_EXPORT extern const char kDisablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kDisableKillAfterBadIPC[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableNewContentRenderingTimeout[];
CONTENT_EXPORT extern const char kDisableNotifications[];
CONTENT_EXPORT extern const char kDisableOriginTrialControlledBlinkFeatures[];
CONTENT_EXPORT extern const char kDisablePartialRaster[];
extern const char kDisablePepper3d[];
CONTENT_EXPORT extern const char kDisablePepper3DImageChromium[];
CONTENT_EXPORT extern const char kDisablePermissionsAPI[];
CONTENT_EXPORT extern const char kDisablePinch[];
CONTENT_EXPORT extern const char kDisablePresentationAPI[];
CONTENT_EXPORT extern const char kDisablePushStateThrottle[];
CONTENT_EXPORT extern const char kDisableRGBA4444Textures[];
CONTENT_EXPORT extern const char kDisableReadingFromCanvas[];
extern const char kDisableRemoteFonts[];
CONTENT_EXPORT extern const char kDisableRemotePlaybackAPI[];
extern const char kDisableRendererAccessibility[];
CONTENT_EXPORT extern const char kDisableRendererBackgrounding[];
CONTENT_EXPORT extern const char kDisableResourceScheduler[];
CONTENT_EXPORT extern const char kDisableSharedWorkers[];
CONTENT_EXPORT extern const char kDisableSkiaRuntimeOpts[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
CONTENT_EXPORT extern const char kDisableSpeechAPI[];
CONTENT_EXPORT extern const char kDisableSpeechSynthesisAPI[];
CONTENT_EXPORT extern const char kDisableTestCerts[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedScrolling[];
extern const char kDisableV8IdleTasks[];
CONTENT_EXPORT extern const char kDisableWebGLImageChromium[];
CONTENT_EXPORT extern const char kDisableWebSecurity[];
CONTENT_EXPORT extern const char kDisableZeroCopy[];
CONTENT_EXPORT extern const char kDisableZeroCopyDxgiVideo[];
CONTENT_EXPORT extern const char kDomAutomationController[];
extern const char kDisable2dCanvasClipAntialiasing[];
CONTENT_EXPORT extern const char kDumpBlinkRuntimeCallStats[];
CONTENT_EXPORT extern const char kEnableAccessibilityObjectModel[];
CONTENT_EXPORT extern const char kEnableAggressiveDOMStorageFlushing[];
CONTENT_EXPORT extern const char kEnableAutomation[];
CONTENT_EXPORT extern const char kEnablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kEnableBlinkFeatures[];
CONTENT_EXPORT extern const char kEnableCaretBrowsing[];
CONTENT_EXPORT extern const char kEnableDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kEnableExperimentalCookieFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebAssemblyFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebPlatformFeatures[];
CONTENT_EXPORT extern const char kEnableFtp[];
CONTENT_EXPORT extern const char kEnableGpuMemoryBufferCompositorResources[];
CONTENT_EXPORT extern const char kEnableGpuMemoryBufferVideoFrames[];
CONTENT_EXPORT extern const char kGpuRasterizationMSAASampleCount[];
CONTENT_EXPORT extern const char kEnableLowResTiling[];
CONTENT_EXPORT extern const char kEnableLCDText[];
CONTENT_EXPORT extern const char kEnableLogging[];
CONTENT_EXPORT extern const char kEnableNetworkInformationDownlinkMax[];
CONTENT_EXPORT extern const char kEnableNewCanvas2DAPI[];
CONTENT_EXPORT extern const char kDisableNv12DxgiVideo[];
CONTENT_EXPORT extern const char kEnablePluginPlaceholderTesting[];
CONTENT_EXPORT extern const char kEnablePreciseMemoryInfo[];
CONTENT_EXPORT extern const char kEnableRGBA4444Textures[];
CONTENT_EXPORT extern const char kEnableServiceBinaryLauncher[];
extern const char kEnableSkiaBenchmarking[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableSpatialNavigation[];
CONTENT_EXPORT extern const char kEnableStrictMixedContentChecking[];
CONTENT_EXPORT extern const char kEnableStrictPowerfulFeatureRestrictions[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTracing[];
CONTENT_EXPORT extern const char kEnableTracingOutput[];
CONTENT_EXPORT extern const char kEnableUserMediaScreenCapturing[];
CONTENT_EXPORT extern const char kEnableUseZoomForDSF[];
CONTENT_EXPORT extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kEnableVtune[];
CONTENT_EXPORT extern const char kEnableWebGL2ComputeContext[];
CONTENT_EXPORT extern const char kEnableWebGLDraftExtensions[];
CONTENT_EXPORT extern const char kEnableWebGLImageChromium[];
CONTENT_EXPORT extern const char kEnableWebVR[];
CONTENT_EXPORT extern const char kEnableZeroCopy[];
CONTENT_EXPORT extern const char kFieldTrialHandle[];
CONTENT_EXPORT extern const char kFileUrlPathAlias[];
CONTENT_EXPORT extern const char kForceDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kDisableOopRasterization[];
CONTENT_EXPORT extern const char kEnableOopRasterization[];
CONTENT_EXPORT extern const char kForceLegacyDefaultReferrerPolicy[];
CONTENT_EXPORT extern const char kForceOverlayFullscreenVideo[];
CONTENT_EXPORT extern const char kForcePresentationReceiverForTesting[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
CONTENT_EXPORT extern const char kGenerateAccessibilityTestExpectations[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
CONTENT_EXPORT extern const char kGpuSandboxStartEarly[];
CONTENT_EXPORT extern const char kGpuStartupDialog[];
CONTENT_EXPORT extern const char kInitialVirtualTime[];
CONTENT_EXPORT extern const char kInProcessGPU[];
CONTENT_EXPORT extern const char kIPCConnectionTimeout[];
CONTENT_EXPORT extern const char kIsolateOrigins[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
CONTENT_EXPORT extern const char kJavaScriptHarmony[];
CONTENT_EXPORT extern const char kLaunchAsBrowser[];
CONTENT_EXPORT extern const char kLogGpuControlListDecisions[];
CONTENT_EXPORT extern const char kLoggingLevel[];
CONTENT_EXPORT extern const char kLogFile[];
CONTENT_EXPORT extern const char kMainFrameResizesAreOrientationChanges[];
extern const char kMaxActiveWebGLContexts[];
extern const char kMaxDecodedImageSizeMb[];
extern const char kMaxUntiledLayerHeight[];
extern const char kMaxUntiledLayerWidth[];
CONTENT_EXPORT extern const char kMessageLoopTypeUi[];
CONTENT_EXPORT extern const char kMHTMLGeneratorOption[];
CONTENT_EXPORT extern const char kMHTMLSkipNostoreMain[];
CONTENT_EXPORT extern const char kMHTMLSkipNostoreAll[];
CONTENT_EXPORT extern const char kMockCertVerifierDefaultResultForTesting[];
CONTENT_EXPORT extern const char kMojoLocalStorage[];
CONTENT_EXPORT extern const char kNetworkQuietTimeout[];
CONTENT_EXPORT extern const char kNoZygote[];
extern const char kNoV8UntrustedCodeMitigations[];
CONTENT_EXPORT extern const char kNumRasterThreads[];
CONTENT_EXPORT extern const char kOverridePluginPowerSaverForTesting[];
CONTENT_EXPORT extern const char kPassiveListenersDefault[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashArgs[];
CONTENT_EXPORT extern const char kPpapiInProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
CONTENT_EXPORT extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kProxyServer[];
CONTENT_EXPORT extern const char kPullToRefresh[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPipe[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRendererClientId[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
CONTENT_EXPORT extern const char kRendererProcessLimit[];
CONTENT_EXPORT extern const char kRendererStartupDialog[];
CONTENT_EXPORT extern const char kRunManualTestsFlag[];
extern const char kSandboxIPCProcess[];
extern const char kShowLayoutShiftRegions[];
extern const char kShowPaintRects[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSitePerProcess[];
CONTENT_EXPORT extern const char kDisableSiteIsolation[];
CONTENT_EXPORT extern const char kStartFullscreen[];
CONTENT_EXPORT extern const char kStatsCollectionController[];
extern const char kSkiaFontCacheLimitMb[];
extern const char kSkiaResourceCacheLimitMb[];
CONTENT_EXPORT extern const char kTestType[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetection[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionAuto[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionEnabled[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionDisabled[];
CONTENT_EXPORT extern const char kTouchTextSelectionStrategy[];
CONTENT_EXPORT extern const char kTrustableWebBundleFileUrl[];
CONTENT_EXPORT extern const char kUseFakeCodecForPeerConnection[];
CONTENT_EXPORT extern const char kUseFakeUIForMediaStream[];
CONTENT_EXPORT extern const char kVideoImageTextureTarget[];
CONTENT_EXPORT extern const char kUseMobileUserAgent[];
CONTENT_EXPORT extern const char kUseMockCertVerifierForTesting[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
CONTENT_EXPORT extern const char kUtilityStartupDialog[];
CONTENT_EXPORT extern const char kUtilitySubType[];
CONTENT_EXPORT extern const char kV8CacheOptions[];
CONTENT_EXPORT extern const char kValidateInputEventStream[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
extern const char kWebglAntialiasingMode[];
extern const char kWebglMSAASampleCount[];

CONTENT_EXPORT extern const char kWebOtpBackend[];
CONTENT_EXPORT extern const char kWebOtpBackendSmsVerification[];
CONTENT_EXPORT extern const char kWebOtpBackendUserConsent[];
CONTENT_EXPORT extern const char kDisableWebRtcEncryption[];
CONTENT_EXPORT extern const char kDisableWebRtcHWDecoding[];
CONTENT_EXPORT extern const char kDisableWebRtcHWEncoding[];
CONTENT_EXPORT extern const char kEnableWebRtcSrtpAesGcm[];
CONTENT_EXPORT extern const char kEnableWebRtcSrtpEncryptedHeaders[];
CONTENT_EXPORT extern const char kEnableWebRtcStunOrigin[];
CONTENT_EXPORT extern const char kEnforceWebRtcIPPermissionCheck[];
CONTENT_EXPORT extern const char kForceWebRtcIPHandlingPolicy[];
CONTENT_EXPORT extern const char kWaitForDebuggerWebUI[];
extern const char kWebRtcMaxCaptureFramerate[];
extern const char kWebRtcMaxCpuConsumptionPercentage[];
CONTENT_EXPORT extern const char kWebRtcStunProbeTrialParameter[];
CONTENT_EXPORT extern const char kWebRtcLocalEventLogging[];
CONTENT_EXPORT extern const char kWebComponentsV0Enabled[];
CONTENT_EXPORT extern const char kUseLegacyFormControls[];
CONTENT_EXPORT extern const char kDisableScrollToTextFragment[];

CONTENT_EXPORT extern const char kWebXrForceRuntime[];
CONTENT_EXPORT extern const char kWebXrRuntimeNone[];
CONTENT_EXPORT extern const char kWebXrRuntimeOrientationSensors[];
CONTENT_EXPORT extern const char kWebXrRuntimeOculus[];
CONTENT_EXPORT extern const char kWebXrRuntimeOpenVr[];
CONTENT_EXPORT extern const char kWebXrRuntimeOpenXr[];
CONTENT_EXPORT extern const char kWebXrRuntimeWMR[];

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kDisableMediaSessionAPI[];
CONTENT_EXPORT extern const char kDisableOverscrollEdgeEffect[];
CONTENT_EXPORT extern const char kDisablePullToRefreshEffect[];
CONTENT_EXPORT extern const char kDisableScreenOrientationLock[];
CONTENT_EXPORT extern const char kDisableSiteIsolationForPolicy[];
CONTENT_EXPORT extern const char kDisableTimeoutsForProfiling[];
CONTENT_EXPORT extern const char kEnableAdaptiveSelectionHandleOrientation[];
CONTENT_EXPORT extern const char kEnableLongpressDragSelection[];
extern const char kNetworkCountryIso[];
CONTENT_EXPORT extern const char kRemoteDebuggingSocketName[];
CONTENT_EXPORT extern const char kRendererWaitForJavaDebugger[];
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kEnableSpeechDispatcher[];
#endif

#if defined(OS_WIN)
CONTENT_EXPORT extern const char kPrefetchArgumentRenderer[];
CONTENT_EXPORT extern const char kPrefetchArgumentGpu[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapi[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapiBroker[];
CONTENT_EXPORT extern const char kPrefetchArgumentOther[];
// This switch contains the device scale factor passed to certain processes
// like renderers, etc.
CONTENT_EXPORT extern const char kDeviceScaleFactor[];
CONTENT_EXPORT extern const char kDisableLegacyIntermediateWindow[];
CONTENT_EXPORT extern const char kEnableWin7WebRtcHWH264Decoding[];
// Switch to pass the font cache shared memory handle to the renderer.
CONTENT_EXPORT extern const char kFontCacheSharedHandle[];
CONTENT_EXPORT extern const char kPpapiAntialiasedTextEnabled[];
CONTENT_EXPORT extern const char kPpapiSubpixelRenderingSetting[];
CONTENT_EXPORT extern const char kGpu2StartupDialog[];
#endif

#if defined(ENABLE_IPC_FUZZER)
extern const char kIpcDumpDirectory[];
extern const char kIpcFuzzerTestcase[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
