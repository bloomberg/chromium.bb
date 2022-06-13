// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_FEATURE_CONSTANTS_H_
#define CHROMECAST_COMMON_FEATURE_CONSTANTS_H_

namespace chromecast {
namespace feature {

// TODO(b/187758538): Upstream more Feature Constants here.

// TODO(b/187524799): Remove this feature when the related features are
// deprecated.
extern const char kEnableTrackControlAppRendererFeatureUse[];
// The app can use playready.
extern const char kEnablePlayready[];

extern const char kKeyAppId[];
// Insecure content is allowed for the app.
extern const char kKeyAllowInsecureContent[];

// If dev mode is enabled, kDevModeOrigin will be set with origin url
extern const char kEnableDevMode[];
extern const char kDevModeOrigin[];

// Permit changes to accessibility control settings?
// (color inversion, high contrast text, etc.)
extern const char kEnableAccessibilityControls[];

// Permit subscription to platform system gesture events?
extern const char kEnableSystemGestures[];

// Enables window APIs for the webpage (show/hide, etc.)
extern const char kEnableWindowControls[];

// Enable mojo connection for the settings UI.
extern const char kEnableSettingsUiMojo[];

// Disable blink background tab timer throttling for making sure application in
// in mini tile mode runs smoothly.
extern const char kDisableBackgroundTabTimerThrottle[];

// Sets RenderFrameMediaPlaybackOptions::kIsBackgroundMediaSuspendEnabled to
// false.
extern const char kDisableBackgroundSuspend[];

// Enable sending/receiving messages to/from libassistant
extern const char kEnableAssistantMessagePipe[];

// Enable a standalone demo app to control privileged features.
extern const char kEnableDemoStandaloneMode[];

}  // namespace feature
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_FEATURE_CONSTANTS_H_
