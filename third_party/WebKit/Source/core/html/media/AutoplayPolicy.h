// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayPolicy_h
#define AutoplayPolicy_h

#include "bindings/core/v8/Nullable.h"
#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
#include "platform/heap/Handle.h"

namespace blink {

class AutoplayUmaHelper;
class Document;
class ElementVisibilityObserver;
class HTMLMediaElement;

// AutoplayPolicy is the class for handles autoplay logics.
class AutoplayPolicy final : public GarbageCollected<AutoplayPolicy> {
 public:
  // Different autoplay policy types.
  enum class Type {
    kNoUserGestureRequired = 0,
    // A local user gesture on the element is required.
    kUserGestureRequired,
    // A local user gesture on the element is required when it is in a cross
    // origin iframe.
    kUserGestureRequiredForCrossOrigin,
    // The document needs to have received a user activation or received one
    // before navigating.
    kDocumentUserActivationRequired,
  };

  CORE_EXPORT static Type GetAutoplayPolicyForDocument(const Document&);
  CORE_EXPORT static bool IsDocumentAllowedToPlay(const Document&);

  explicit AutoplayPolicy(HTMLMediaElement*);

  void VideoWillBeDrawnToCanvas() const;

  // Called when the media element is moved to a new document.
  void DidMoveToNewDocument(Document& old_document);

  // Stop autoplaying the video element whenever its visible.
  // TODO(mlamouri): hide these methods from HTMLMediaElement.
  void StopAutoplayMutedWhenVisible();

  // Request autoplay by attribute. This method will check the autoplay
  // restrictions and record metrics. This method can only be called once per
  // time the readyState changes to HAVE_ENOUGH_DATA.
  bool RequestAutoplayByAttribute();

  // Request the playback via play() method. This method will check the autoplay
  // restrictions and record metrics. This method can only be called once
  // per call of play().
  Nullable<ExceptionCode> RequestPlay();

  // Returns whether an umute action should pause an autoplaying element. The
  // method will check autoplay restrictions and record metrics. This method can
  // only be called once per call of setMuted().
  bool RequestAutoplayUnmute();

  // Indicates the media element is autoplaying because of being muted.
  bool IsAutoplayingMuted() const;

  // Indicates the media element is or will autoplay because of being
  // muted.
  CORE_EXPORT bool IsOrWillBeAutoplayingMuted() const;

  // Unlock user gesture if a user gesture can be utilized.
  void TryUnlockingUserGesture();

  // Return true if and only if a user gesture is requried for playback.  Even
  // if isLockedPendingUserGesture() return true, this might return false if
  // the requirement is currently overridden.  This does not check if a user
  // gesture is currently being processed.
  bool IsGestureNeededForPlayback() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class AutoplayUmaHelper;
  friend class AutoplayUmaHelperTest;

  // Start autoplaying the video element whenever its visible.
  void StartAutoplayMutedWhenVisible();

  // Returns whether the media element is eligible to autoplay muted.
  bool IsEligibleForAutoplayMuted() const;

  bool ShouldAutoplay();

  // If the user gesture is required, then this will remove it.  Note that
  // one should not generally call this method directly; use the one on
  // m_helper and give it a reason.
  void UnlockUserGesture();

  // Return true if and only if a user gesture is required to unlock this
  // media element for unrestricted autoplay/script control.  Don't confuse
  // this with isGestureNeededForPlayback().  The latter is usually what one
  // should use, if checking to see if an action is allowed.
  bool IsLockedPendingUserGesture() const;

  bool IsLockedPendingUserGestureIfCrossOriginExperimentEnabled() const;

  bool IsGestureNeededForPlaybackIfCrossOriginExperimentEnabled() const;

  bool IsGestureNeededForPlaybackIfPendingUserGestureIsLocked() const;

  // Return true if and only if the settings allow autoplay of media on this
  // frame.
  bool IsAutoplayAllowedPerSettings() const;

  bool IsAutoplayingMutedInternal(bool muted) const;
  bool IsOrWillBeAutoplayingMutedInternal(bool muted) const;

  // Called when the video visibility changes while autoplaying muted, will
  // pause the video when invisible and resume the video when visible.
  void OnVisibilityChangedForAutoplay(bool is_visible);

  bool locked_pending_user_gesture_ : 1;
  bool locked_pending_user_gesture_if_cross_origin_experiment_enabled_ : 1;

  Member<HTMLMediaElement> element_;
  Member<ElementVisibilityObserver> autoplay_visibility_observer_;

  Member<AutoplayUmaHelper> autoplay_uma_helper_;

  DISALLOW_COPY_AND_ASSIGN(AutoplayPolicy);
};

}  // namespace blink

#endif  // AutoplayPolicy_h
