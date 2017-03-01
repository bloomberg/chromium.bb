// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsOrientationLockDelegate_h
#define MediaControlsOrientationLockDelegate_h

#include "core/CoreExport.h"
#include "core/events/EventListener.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

namespace blink {

class Document;
class HTMLVideoElement;

// MediaControlsOrientationLockDelegate is implementing the orientation lock
// feature when a <video> is fullscreen. It is meant to be created by
// `MediaControls` when the feature applies. Once created, it will use events to
// change state.
//
// The different states of the class are:
// - PendingFullscreen: the object is created and is waiting for the associated
//   <video> to go fullscreen in order to apply an orientation lock;
// - PendingMetadata: the <video> is fullscreen but the metadata have not been
//   downloaded yet. It can happen because of network latency or because the
//   <video> went fullscreen before playback and download started;
// - MaybeLockedFullscreen: the <video> is fullscreen and a screen orientation
//   lock was applied.
//
// The possible state transitions are:
// - PendingFullscreen => PendingMetadata: on fullscreenchange event (entering
//   fullscreen) when metadata are not available;
// - PendingFullscreen => MaybeLockedFullscreen: on fullscreenchange event
//   (entering fullscreen) when metadata are available;
// - PendingMetadata => MaybeLockedFullscreen: on loadedmetadata;
// - PendingMetadata => PendingFullscreen: on fullscreenchange event (leaving
//   fullscreen);
// - MaybeLockedFullscreen => PendingFullscreen: on fullscreenchange event
//   (leaving fullscreen).
class CORE_EXPORT MediaControlsOrientationLockDelegate final
    : public EventListener {
 public:
  explicit MediaControlsOrientationLockDelegate(HTMLVideoElement&);

  // Called by MediaControls when the HTMLMediaElement is added to a document
  // document. All event listeners should be added.
  void attach();

  // Called by MediaControls when the HTMLMediaElement is no longer in the
  // document. All event listeners should be removed in order to prepare the
  // object to be garbage collected.
  void detach();

  // EventListener implementation.
  bool operator==(const EventListener&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaControlsOrientationLockDelegateTest;

  enum class State {
    PendingFullscreen,
    PendingMetadata,
    MaybeLockedFullscreen,
  };

  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;

  HTMLVideoElement& videoElement() const;
  Document& document() const;

  // Returns the orientation in which the video should be locked based on its
  // size.
  WebScreenOrientationLockType computeOrientationLock() const;

  // Locks the screen orientation if the video has metadata information
  // available. Delays locking orientation until metadata are available
  // otherwise.
  void maybeLockOrientation();

  // Unlocks the screen orientation if the screen orientation was previously
  // locked.
  void maybeUnlockOrientation();

  // Current state of the object. See comment at the top of the file for a
  // detailed description.
  State m_state = State::PendingFullscreen;

  // Whether the controls should unlock the screen orientation when possible.
  // In other words, whether the orientation was locked.
  bool m_shouldUnlockOrientation = false;

  // `m_videoElement` owns MediaControls that owns |this|.
  Member<HTMLVideoElement> m_videoElement;
};

}  // namespace blink

#endif  // MediaControlsOrientationLockDelegate_h
