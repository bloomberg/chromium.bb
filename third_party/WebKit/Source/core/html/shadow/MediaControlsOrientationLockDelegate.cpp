// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "MediaControlsOrientationLockDelegate.h"

#include "core/events/Event.h"
#include "core/frame/ScreenOrientationController.h"
#include "core/html/HTMLVideoElement.h"
#include "core/page/ChromeClient.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"

namespace blink {

namespace {

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback : public WebLockOrientationCallback {
 public:
  void onSuccess() override {}
  void onError(WebLockOrientationError) override {}
};

}  // anonymous namespace

MediaControlsOrientationLockDelegate::MediaControlsOrientationLockDelegate(
    HTMLVideoElement& video)
    : EventListener(CPPEventListenerType), m_videoElement(video) {
  document().addEventListener(EventTypeNames::fullscreenchange, this, true);
  videoElement().addEventListener(EventTypeNames::webkitfullscreenchange, this,
                                  true);
  videoElement().addEventListener(EventTypeNames::loadedmetadata, this, true);
}

bool MediaControlsOrientationLockDelegate::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsOrientationLockDelegate::maybeLockOrientation() {
  DCHECK(m_state != State::MaybeLockedFullscreen);

  if (videoElement().getReadyState() == HTMLMediaElement::kHaveNothing) {
    m_state = State::PendingMetadata;
    return;
  }

  m_state = State::MaybeLockedFullscreen;

  if (!document().frame())
    return;

  auto controller = ScreenOrientationController::from(*document().frame());
  if (controller->maybeHasActiveLock())
    return;

  controller->lock(computeOrientationLock(),
                   WTF::wrapUnique(new DummyScreenOrientationCallback));
  m_shouldUnlockOrientation = true;
}

void MediaControlsOrientationLockDelegate::maybeUnlockOrientation() {
  DCHECK(m_state != State::PendingFullscreen);

  m_state = State::PendingFullscreen;

  if (!m_shouldUnlockOrientation)
    return;

  ScreenOrientationController::from(*document().frame())->unlock();
  m_shouldUnlockOrientation = false;
}

HTMLVideoElement& MediaControlsOrientationLockDelegate::videoElement() const {
  return *m_videoElement;
}

Document& MediaControlsOrientationLockDelegate::document() const {
  return videoElement().document();
}

void MediaControlsOrientationLockDelegate::handleEvent(
    ExecutionContext* executionContext,
    Event* event) {
  if (event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    if (videoElement().isFullscreen()) {
      if (m_state == State::PendingFullscreen)
        maybeLockOrientation();
    } else {
      if (m_state != State::PendingFullscreen)
        maybeUnlockOrientation();
    }

    return;
  }

  if (event->type() == EventTypeNames::loadedmetadata) {
    if (m_state == State::PendingMetadata)
      maybeLockOrientation();

    return;
  }

  NOTREACHED();
}

WebScreenOrientationLockType
MediaControlsOrientationLockDelegate::computeOrientationLock() const {
  DCHECK(videoElement().getReadyState() != HTMLMediaElement::kHaveNothing);

  const unsigned width = videoElement().videoWidth();
  const unsigned height = videoElement().videoHeight();

  if (width > height)
    return WebScreenOrientationLockLandscape;

  if (height > width)
    return WebScreenOrientationLockPortrait;

  // For square videos, try to lock to the current screen orientation for
  // consistency. Use WebScreenOrientationLockLandscape as a fallback value.
  // TODO(mlamouri): we could improve this by having direct access to
  // `window.screen.orientation.type`.
  Frame* frame = document().frame();
  if (!frame)
    return WebScreenOrientationLockLandscape;

  switch (frame->chromeClient().screenInfo().orientationType) {
    case WebScreenOrientationPortraitPrimary:
    case WebScreenOrientationPortraitSecondary:
      return WebScreenOrientationLockPortrait;
    case WebScreenOrientationLandscapePrimary:
    case WebScreenOrientationLandscapeSecondary:
      return WebScreenOrientationLockLandscape;
    case WebScreenOrientationUndefined:
      return WebScreenOrientationLockLandscape;
  }

  NOTREACHED();
  return WebScreenOrientationLockLandscape;
}

DEFINE_TRACE(MediaControlsOrientationLockDelegate) {
  EventListener::trace(visitor);
  visitor->trace(m_videoElement);
}

}  // namespace blink
