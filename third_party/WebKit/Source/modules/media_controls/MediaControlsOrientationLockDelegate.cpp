// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsOrientationLockDelegate.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Screen.h"
#include "core/frame/ScreenOrientationController.h"
#include "core/html/HTMLVideoElement.h"
#include "core/page/ChromeClient.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/device_orientation/DeviceOrientationEvent.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "modules/screen_orientation/ScreenScreenOrientation.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/MathExtras.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"

#if OS(ANDROID)
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "services/device/public/interfaces/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#endif  // OS(ANDROID)

#undef atan2  // to use std::atan2 instead of wtf_atan2
#undef fmod   // to use std::fmod instead of wtf_fmod
#include <cmath>

namespace blink {

namespace {

// These values are used for histograms. Do not reorder.
enum class MetadataAvailabilityMetrics {
  kAvailable = 0,  // Available when lock was attempted.
  kMissing = 1,    // Missing when lock was attempted.
  kReceived = 2,   // Received after being missing in order to lock.

  // Keep at the end.
  kMax = 3
};

// These values are used for histograms. Do not reorder.
enum class LockResultMetrics {
  kAlreadyLocked = 0,  // Frame already has a lock.
  kPortrait = 1,       // Locked to portrait.
  kLandscape = 2,      // Locked to landscape.

  // Keep at the end.
  kMax = 3
};

void RecordMetadataAvailability(MetadataAvailabilityMetrics metrics) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, metadata_histogram,
      ("Media.Video.FullscreenOrientationLock.MetadataAvailability",
       static_cast<int>(MetadataAvailabilityMetrics::kMax)));
  metadata_histogram.Count(static_cast<int>(metrics));
}

void RecordLockResult(LockResultMetrics metrics) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, lock_result_histogram,
                      ("Media.Video.FullscreenOrientationLock.LockResult",
                       static_cast<int>(LockResultMetrics::kMax)));
  lock_result_histogram.Count(static_cast<int>(metrics));
}

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback : public WebLockOrientationCallback {
 public:
  void OnSuccess() override {}
  void OnError(WebLockOrientationError) override {}
};

}  // anonymous namespace

MediaControlsOrientationLockDelegate::MediaControlsOrientationLockDelegate(
    HTMLVideoElement& video)
    : EventListener(kCPPEventListenerType), video_element_(video) {
  if (VideoElement().isConnected())
    Attach();
}

void MediaControlsOrientationLockDelegate::Attach() {
  DCHECK(VideoElement().isConnected());

  GetDocument().addEventListener(EventTypeNames::fullscreenchange, this, true);
  VideoElement().addEventListener(EventTypeNames::webkitfullscreenchange, this,
                                  true);
  VideoElement().addEventListener(EventTypeNames::loadedmetadata, this, true);
}

void MediaControlsOrientationLockDelegate::Detach() {
  DCHECK(!VideoElement().isConnected());

  GetDocument().removeEventListener(EventTypeNames::fullscreenchange, this,
                                    true);
  VideoElement().removeEventListener(EventTypeNames::webkitfullscreenchange,
                                     this, true);
  VideoElement().removeEventListener(EventTypeNames::loadedmetadata, this,
                                     true);
}

bool MediaControlsOrientationLockDelegate::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsOrientationLockDelegate::MaybeLockOrientation() {
  DCHECK(state_ != State::kMaybeLockedFullscreen);

  if (VideoElement().getReadyState() == HTMLMediaElement::kHaveNothing) {
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kMissing);
    state_ = State::kPendingMetadata;
    return;
  }

  if (state_ == State::kPendingMetadata)
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kReceived);
  else
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kAvailable);

  state_ = State::kMaybeLockedFullscreen;

  if (!GetDocument().GetFrame())
    return;

  auto controller =
      ScreenOrientationController::From(*GetDocument().GetFrame());
  if (controller->MaybeHasActiveLock()) {
    RecordLockResult(LockResultMetrics::kAlreadyLocked);
    return;
  }

  locked_orientation_ = ComputeOrientationLock();
  DCHECK_NE(locked_orientation_, kWebScreenOrientationLockDefault);
  controller->lock(locked_orientation_,
                   WTF::WrapUnique(new DummyScreenOrientationCallback));

  if (locked_orientation_ == kWebScreenOrientationLockLandscape)
    RecordLockResult(LockResultMetrics::kLandscape);
  else
    RecordLockResult(LockResultMetrics::kPortrait);

  MaybeListenToDeviceOrientation();
}

void MediaControlsOrientationLockDelegate::MaybeUnlockOrientation() {
  DCHECK(state_ != State::kPendingFullscreen);

  state_ = State::kPendingFullscreen;

  if (locked_orientation_ == kWebScreenOrientationLockDefault /* unlocked */)
    return;

  monitor_.reset();  // Cancel any GotIsAutoRotateEnabledByUser Mojo callback.
  if (LocalDOMWindow* dom_window = GetDocument().domWindow()) {
    dom_window->removeEventListener(EventTypeNames::deviceorientation, this,
                                    false);
  }

  ScreenOrientationController::From(*GetDocument().GetFrame())->unlock();
  locked_orientation_ = kWebScreenOrientationLockDefault /* unlocked */;

  unlock_task_.Cancel();
}

void MediaControlsOrientationLockDelegate::MaybeListenToDeviceOrientation() {
  DCHECK_EQ(state_, State::kMaybeLockedFullscreen);
  DCHECK_NE(locked_orientation_, kWebScreenOrientationLockDefault);

  // If the rotate-to-fullscreen feature is also enabled, then start listening
  // to deviceorientation events so the orientation can be unlocked once the
  // user rotates the device to match the video's orientation (allowing the user
  // to then exit fullscreen by rotating their device back to the opposite
  // orientation). Otherwise, don't listen for deviceorientation events and just
  // hold the orientation lock until the user exits fullscreen (which prevents
  // the user rotating to the wrong fullscreen orientation).
  if (!RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled())
    return;

  if (is_auto_rotate_enabled_by_user_override_for_testing_ != WTF::nullopt) {
    GotIsAutoRotateEnabledByUser(
        is_auto_rotate_enabled_by_user_override_for_testing_.value());
    return;
  }

// Check whether the user locked screen orientation at the OS level.
#if OS(ANDROID)
  DCHECK(!monitor_.is_bound());
  Platform::Current()->GetConnector()->BindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&monitor_));
  monitor_->IsAutoRotateEnabledByUser(ConvertToBaseCallback(WTF::Bind(
      &MediaControlsOrientationLockDelegate::GotIsAutoRotateEnabledByUser,
      WrapPersistent(this))));
#else
  GotIsAutoRotateEnabledByUser(true);  // Assume always enabled on other OSes.
#endif  // OS(ANDROID)
}

void MediaControlsOrientationLockDelegate::GotIsAutoRotateEnabledByUser(
    bool enabled) {
  monitor_.reset();

  if (!enabled) {
    // Since the user has locked their screen orientation, prevent
    // MediaControlsRotateToFullscreenDelegate from exiting fullscreen by not
    // listening for deviceorientation events and instead continuing to hold the
    // orientation lock until the user exits fullscreen. This enables users to
    // watch videos in bed with their head facing sideways (which requires a
    // landscape screen orientation when the device is portrait and vice versa).
    // TODO(johnme): Ideally we would start listening for deviceorientation
    // events and allow rotating to exit if a user enables screen auto rotation
    // after we have locked to landscape. That would require listening for
    // changes to the auto rotate setting, rather than only checking it once.
    return;
  }

  if (LocalDOMWindow* dom_window = GetDocument().domWindow()) {
    dom_window->addEventListener(EventTypeNames::deviceorientation, this,
                                 false);
  }
}

HTMLVideoElement& MediaControlsOrientationLockDelegate::VideoElement() const {
  return *video_element_;
}

Document& MediaControlsOrientationLockDelegate::GetDocument() const {
  return VideoElement().GetDocument();
}

void MediaControlsOrientationLockDelegate::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    if (VideoElement().IsFullscreen()) {
      if (state_ == State::kPendingFullscreen)
        MaybeLockOrientation();
    } else {
      if (state_ != State::kPendingFullscreen)
        MaybeUnlockOrientation();
    }

    return;
  }

  if (event->type() == EventTypeNames::loadedmetadata) {
    if (state_ == State::kPendingMetadata)
      MaybeLockOrientation();

    return;
  }

  if (event->type() == EventTypeNames::deviceorientation) {
    MaybeUnlockIfDeviceOrientationMatchesVideo(ToDeviceOrientationEvent(event));

    return;
  }

  NOTREACHED();
}

WebScreenOrientationLockType
MediaControlsOrientationLockDelegate::ComputeOrientationLock() const {
  DCHECK(VideoElement().getReadyState() != HTMLMediaElement::kHaveNothing);

  const unsigned width = VideoElement().videoWidth();
  const unsigned height = VideoElement().videoHeight();

  if (width > height)
    return kWebScreenOrientationLockLandscape;

  if (height > width)
    return kWebScreenOrientationLockPortrait;

  // For square videos, try to lock to the current screen orientation for
  // consistency. Use WebScreenOrientationLockLandscape as a fallback value.
  // TODO(mlamouri): we could improve this by having direct access to
  // `window.screen.orientation.type`.
  Frame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationLockLandscape;

  switch (frame->GetChromeClient().GetScreenInfo().orientation_type) {
    case kWebScreenOrientationPortraitPrimary:
    case kWebScreenOrientationPortraitSecondary:
      return kWebScreenOrientationLockPortrait;
    case kWebScreenOrientationLandscapePrimary:
    case kWebScreenOrientationLandscapeSecondary:
      return kWebScreenOrientationLockLandscape;
    case kWebScreenOrientationUndefined:
      return kWebScreenOrientationLockLandscape;
  }

  NOTREACHED();
  return kWebScreenOrientationLockLandscape;
}

MediaControlsOrientationLockDelegate::DeviceOrientationType
MediaControlsOrientationLockDelegate::ComputeDeviceOrientation(
    DeviceOrientationData* data) const {
  LocalDOMWindow* dom_window = GetDocument().domWindow();
  if (!dom_window)
    return DeviceOrientationType::kUnknown;

  if (!data->CanProvideBeta() || !data->CanProvideGamma())
    return DeviceOrientationType::kUnknown;
  double beta = data->Beta();
  double gamma = data->Gamma();

  // Calculate the projection of the up vector (normal to the earth's surface)
  // onto the device's screen in its natural orientation. (x,y) will lie within
  // the unit circle centered on (0,0), e.g. if the top of the device is
  // pointing upwards (x,y) will be (0,-1).
  double x = -std::sin(deg2rad(gamma)) * std::cos(deg2rad(beta));
  double y = -std::sin(deg2rad(beta));

  // Convert (x,y) to polar coordinates: 0 <= device_orientation_angle < 360 and
  // 0 <= r <= 1, such that device_orientation_angle is the clockwise angle in
  // degrees between the current physical orientation of the device and the
  // natural physical orientation of the device (ignoring the screen
  // orientation). Thus snapping device_orientation_angle to the nearest
  // multiple of 90 gives the value screen.orientation.angle would have if the
  // screen orientation was allowed to rotate freely to match the device
  // orientation. Note that we want device_orientation_angle==0 when the top of
  // the device is pointing upwards, but atan2's zero angle points to the right,
  // so we pass y=x and x=-y to atan2 to rotate by 90 degrees.
  double r = std::sqrt(x * x + y * y);
  double device_orientation_angle =
      std::fmod(rad2deg(std::atan2(/* y= */ x, /* x= */ -y)) + 360, 360);

  // If angle between device's screen and the horizontal plane is less than
  // kMinElevationAngle (chosen to approximately match Android's behavior), then
  // device is too flat to reliably determine orientation.
  constexpr double kMinElevationAngle = 24;  // degrees from horizontal plane
  if (r < std::sin(deg2rad(kMinElevationAngle)))
    return DeviceOrientationType::kFlat;

  // device_orientation_angle snapped to nearest multiple of 90.
  int device_orientation_angle90 =
      std::lround(device_orientation_angle / 90) * 90;

  // To be considered portrait or landscape, allow the device to be rotated 23
  // degrees (chosen to approximately match Android's behavior) to either side
  // of those orientations. In the remaining 90 - 2*23 = 44 degree hysteresis
  // zones, consider the device to be diagonal. These hysteresis zones prevent
  // the computed orientation from oscillating rapidly between portrait and
  // landscape when the device is in between the two orientations.
  if (std::abs(device_orientation_angle - device_orientation_angle90) > 23)
    return DeviceOrientationType::kDiagonal;

  // screen.orientation.angle is the standardized replacement for
  // window.orientation. They are equal, except -90 was replaced by 270.
  int screen_orientation_angle =
      ScreenScreenOrientation::orientation(nullptr /* ScriptState */,
                                           *dom_window->screen())
          ->angle();

  // This is equivalent to screen.orientation.type.startsWith('landscape').
  bool screen_orientation_is_portrait =
      dom_window->screen()->width() <= dom_window->screen()->height();

  // The natural orientation of the device could either be portrait (almost
  // all phones, and some tablets like Nexus 7) or landscape (other tablets
  // like Pixel C). Detect this by comparing angle to orientation.
  // TODO(johnme): This might get confused on square screens.
  bool screen_orientation_is_natural_or_flipped_natural =
      screen_orientation_angle % 180 == 0;
  bool natural_orientation_is_portrait =
      screen_orientation_is_portrait ==
      screen_orientation_is_natural_or_flipped_natural;

  // If natural_orientation_is_portrait_, then angles 0 and 180 are portrait,
  // otherwise angles 90 and 270 are portrait.
  int portrait_angle_mod_180 = natural_orientation_is_portrait ? 0 : 90;
  return device_orientation_angle90 % 180 == portrait_angle_mod_180
             ? DeviceOrientationType::kPortrait
             : DeviceOrientationType::kLandscape;
}

void MediaControlsOrientationLockDelegate::
    MaybeUnlockIfDeviceOrientationMatchesVideo(DeviceOrientationEvent* event) {
  DCHECK_EQ(state_, State::kMaybeLockedFullscreen);
  DCHECK(locked_orientation_ == kWebScreenOrientationLockPortrait ||
         locked_orientation_ == kWebScreenOrientationLockLandscape);

  DeviceOrientationType device_orientation =
      ComputeDeviceOrientation(event->Orientation());

  DeviceOrientationType video_orientation =
      locked_orientation_ == kWebScreenOrientationLockPortrait
          ? DeviceOrientationType::kPortrait
          : DeviceOrientationType::kLandscape;

  if (device_orientation != video_orientation)
    return;

  // Job done: the user rotated their device to match the orientation of the
  // video that we locked to, so now we can stop listening and unlock.
  if (LocalDOMWindow* dom_window = GetDocument().domWindow()) {
    dom_window->removeEventListener(EventTypeNames::deviceorientation, this,
                                    false);
  }
  // Delay before unlocking, as a workaround for the case where the device is
  // initially portrait-primary, then fullscreen orientation lock locks it to
  // landscape and the screen orientation changes to landscape-primary, but the
  // user actually rotates the device to landscape-secondary. In that case, if
  // this delegate unlocks the orientation before Android has detected the
  // rotation to landscape-secondary (which is slow due to low-pass filtering),
  // Android would change the screen orientation back to portrait-primary. This
  // is avoided by delaying unlocking long enough to ensure that Android has
  // detected the orientation change.
  unlock_task_ =
      TaskRunnerHelper::Get(TaskType::kMediaElementEvent, &GetDocument())
          ->PostDelayedCancellableTask(
              BLINK_FROM_HERE,
              WTF::Bind(
                  &MediaControlsOrientationLockDelegate::MaybeUnlockOrientation,
                  WrapPersistent(this)),
              TimeDelta::FromMilliseconds(kUnlockDelayMs));
}

DEFINE_TRACE(MediaControlsOrientationLockDelegate) {
  EventListener::Trace(visitor);
  visitor->Trace(video_element_);
}

}  // namespace blink
