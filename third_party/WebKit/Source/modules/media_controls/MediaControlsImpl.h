/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlsImpl_h
#define MediaControlsImpl_h

#include "core/html/HTMLDivElement.h"
#include "core/html/media/MediaControls.h"
#include "modules/ModulesExport.h"

namespace blink {

class Event;
class MediaControlsMediaEventListener;
class MediaControlsOrientationLockDelegate;
class MediaControlsRotateToFullscreenDelegate;
class MediaControlsWindowEventListener;
class MediaControlCastButtonElement;
class MediaControlCurrentTimeDisplayElement;
class MediaControlDownloadButtonElement;
class MediaControlFullscreenButtonElement;
class MediaControlMuteButtonElement;
class MediaControlOverflowMenuButtonElement;
class MediaControlOverflowMenuListElement;
class MediaControlOverlayEnclosureElement;
class MediaControlOverlayPlayButtonElement;
class MediaControlPanelElement;
class MediaControlPanelEnclosureElement;
class MediaControlPlayButtonElement;
class MediaControlRemainingTimeDisplayElement;
class MediaControlTextTrackListElement;
class MediaControlTimelineElement;
class MediaControlToggleClosedCaptionsButtonElement;
class MediaControlVolumeSliderElement;
class ShadowRoot;

// Default implementation of the core/ MediaControls interface used by
// HTMLMediaElement.
class MODULES_EXPORT MediaControlsImpl final : public HTMLDivElement,
                                               public MediaControls {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsImpl);
  WTF_MAKE_NONCOPYABLE(MediaControlsImpl);

 public:
  class Factory : public MediaControls::Factory {
   public:
    MediaControls* Create(HTMLMediaElement&, ShadowRoot&) override;
  };

  ~MediaControlsImpl() = default;

  // Node override.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  // MediaControls implementation.
  void MaybeShow() override;
  void Hide() override;
  void Reset() override;
  void OnControlsListUpdated() override;
  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  void OnTrackElementFailedToLoad() override { OnTextTracksAddedOrRemoved(); }
  // Notify us that the media element's network state has changed.
  void NetworkStateChanged() override;
  LayoutObject* PanelLayoutObject() override;
  LayoutObject* ContainerLayoutObject() override;
  // Return the internal elements, which is used by registering clicking
  // EventHandlers from MediaControlsWindowEventListener.
  HTMLDivElement* PanelElement() override;
  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the `MediaControlsEnabled` setting has changed.
  void OnMediaControlsEnabledChange() override {
    // There is no update because only the overlay is expected to change.
    RefreshCastButtonVisibilityWithoutUpdate();
  }
  Document& OwnerDocument() { return GetDocument(); }

  // Called by the fullscreen buttons to toggle fulllscreen on/off.
  void EnterFullscreen();
  void ExitFullscreen();

  // Text track related methods exposed to components handling closed captions.
  void ToggleTextTrackList();
  void ShowTextTrackAtIndex(unsigned);
  void DisableShowingTextTracks();

  // Methods related to the overflow menu.
  void ToggleOverflowMenu();
  bool OverflowMenuVisible();

  void ShowOverlayCastButtonIfNeeded();

  // Methods call by the scrubber.
  void BeginScrubbing();
  void EndScrubbing();
  void UpdateCurrentTimeDisplay();

  DECLARE_VIRTUAL_TRACE();

 private:
  // MediaControlsMediaEventListener is a component that is listening to events
  // and calling the appropriate callback on MediaControlsImpl. The object is
  // split from MedaiControlsImpl to reduce boilerplate and ease reading. In
  // order to not expose accessors only for this component, a friendship is
  // declared.
  friend class MediaControlsMediaEventListener;
  // Same as above but handles the menus hiding when the window is interacted
  // with, allowing MediaControlsImpl to not have the boilerplate.
  friend class MediaControlsWindowEventListener;

  // For tests.
  friend class MediaControlsOrientationLockDelegateTest;
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;
  friend class MediaControlsRotateToFullscreenDelegateTest;
  friend class MediaControlsImplTest;

  // Need to be members of MediaControls for private member access.
  class BatchedControlUpdate;
  class MediaControlsResizeObserverDelegate;
  class MediaElementMutationCallback;

  static MediaControlsImpl* Create(HTMLMediaElement&, ShadowRoot&);

  void Invalidate(Element*);

  // Notify us that our controls enclosure has changed size.
  void NotifyElementSizeChanged(ClientRect* new_size);

  explicit MediaControlsImpl(HTMLMediaElement&);

  void InitializeControls();

  void MakeOpaque();
  void MakeTransparent();
  bool IsVisible() const;

  void UpdatePlayState();

  enum HideBehaviorFlags {
    kIgnoreNone = 0,
    kIgnoreVideoHover = 1 << 0,
    kIgnoreFocus = 1 << 1,
    kIgnoreControlsHover = 1 << 2,
    kIgnoreWaitForTimer = 1 << 3,
  };

  bool ShouldHideMediaControls(unsigned behavior_flags = 0) const;
  void HideMediaControlsTimerFired(TimerBase*);
  void StartHideMediaControlsTimer();
  void StopHideMediaControlsTimer();
  void ResetHideMediaControlsTimer();

  void ElementSizeChangedTimerFired(TimerBase*);

  void HideAllMenus();

  // Hide elements that don't fit, and show those things that we want which
  // do fit.  This requires that m_effectiveWidth and m_effectiveHeight are
  // current.
  void ComputeWhichControlsFit();

  // Node
  bool IsMediaControls() const override { return true; }
  bool WillRespondToMouseMoveEvents() override { return true; }
  void DefaultEventHandler(Event*) override;
  bool ContainsRelatedTarget(Event*);

  // Internal cast related methods.
  void RemotePlaybackStateChanged();
  void RefreshCastButtonVisibility();
  void RefreshCastButtonVisibilityWithoutUpdate();

  // Methods called by MediaControlsMediaEventListener.
  void OnVolumeChange();
  void OnFocusIn();
  void OnTimeUpdate();
  void OnDurationChange();
  void OnPlay();
  void OnPlaying();
  void OnPause();
  void OnTextTracksAddedOrRemoved();
  void OnTextTracksChanged();
  void OnError();
  void OnLoadedMetadata();
  void OnEnteredFullscreen();
  void OnExitedFullscreen();
  void OnPanelKeypress();
  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(event); }

  // Media control elements.
  Member<MediaControlOverlayEnclosureElement> overlay_enclosure_;
  Member<MediaControlOverlayPlayButtonElement> overlay_play_button_;
  Member<MediaControlCastButtonElement> overlay_cast_button_;
  Member<MediaControlPanelEnclosureElement> enclosure_;
  Member<MediaControlPanelElement> panel_;
  Member<MediaControlPlayButtonElement> play_button_;
  Member<MediaControlTimelineElement> timeline_;
  Member<MediaControlCurrentTimeDisplayElement> current_time_display_;
  Member<MediaControlRemainingTimeDisplayElement> duration_display_;
  Member<MediaControlMuteButtonElement> mute_button_;
  Member<MediaControlVolumeSliderElement> volume_slider_;
  Member<MediaControlToggleClosedCaptionsButtonElement>
      toggle_closed_captions_button_;
  Member<MediaControlTextTrackListElement> text_track_list_;
  Member<MediaControlOverflowMenuButtonElement> overflow_menu_;
  Member<MediaControlOverflowMenuListElement> overflow_list_;

  Member<MediaControlCastButtonElement> cast_button_;
  Member<MediaControlFullscreenButtonElement> fullscreen_button_;
  Member<MediaControlDownloadButtonElement> download_button_;

  Member<MediaControlsMediaEventListener> media_event_listener_;
  Member<MediaControlsWindowEventListener> window_event_listener_;
  Member<MediaControlsOrientationLockDelegate> orientation_lock_delegate_;
  Member<MediaControlsRotateToFullscreenDelegate>
      rotate_to_fullscreen_delegate_;

  TaskRunnerTimer<MediaControlsImpl> hide_media_controls_timer_;
  unsigned hide_timer_behavior_flags_;
  bool is_mouse_over_controls_ : 1;
  bool is_paused_for_scrubbing_ : 1;

  // Watches the video element for resize and updates media controls as
  // necessary.
  Member<ResizeObserver> resize_observer_;

  // Watches the media element for attribute changes and updates media controls
  // as necessary.
  Member<MediaElementMutationCallback> element_mutation_callback_;

  TaskRunnerTimer<MediaControlsImpl> element_size_changed_timer_;
  IntSize size_;

  bool keep_showing_until_timer_fires_ : 1;
};

DEFINE_ELEMENT_TYPE_CASTS(MediaControlsImpl, IsMediaControls());

}  // namespace blink

#endif
