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
#include "core/html/shadow/MediaControlElements.h"
#include "modules/ModulesExport.h"

namespace blink {

class Event;
class MediaControlsMediaEventListener;
class MediaControlsOrientationLockDelegate;
class MediaControlsWindowEventListener;
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
    MediaControls* create(HTMLMediaElement&, ShadowRoot&) override;
  };

  ~MediaControlsImpl() = default;

  // Node override.
  Node::InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;

  // MediaControls implementation.
  void show() override;
  void hide() override;
  void reset() override;
  void onControlsListUpdated() override;
  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  void onTrackElementFailedToLoad() override { onTextTracksAddedOrRemoved(); }
  // TODO(mlamouri): the following methods will be able to become private when
  // the controls have to modules/ and have access to RemotePlayback.
  void onRemotePlaybackAvailabilityChanged() override {
    refreshCastButtonVisibility();
  }
  void onRemotePlaybackConnecting() override { startedCasting(); }
  void onRemotePlaybackDisconnected() override { stoppedCasting(); }
  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the attribute have changed.
  void onDisableRemotePlaybackAttributeChanged() override {
    refreshCastButtonVisibility();
  }
  // Notify us that the media element's network state has changed.
  void networkStateChanged() override;
  LayoutObject* panelLayoutObject() override;
  LayoutObject* containerLayoutObject() override;
  // Return the internal elements, which is used by registering clicking
  // EventHandlers from MediaControlsWindowEventListener.
  MediaControlPanelElement* panelElement() override { return m_panel; }
  MediaControlTimelineElement* timelineElement() override { return m_timeline; }
  MediaControlCastButtonElement* castButtonElement() override {
    return m_castButton;
  }
  MediaControlVolumeSliderElement* volumeSliderElement() override {
    return m_volumeSlider;
  }
  void beginScrubbing() override;
  void endScrubbing() override;
  void updateCurrentTimeDisplay() override;
  void toggleTextTrackList() override;
  void showTextTrackAtIndex(unsigned indexToEnable) override;
  void disableShowingTextTracks() override;
  // Called by the fullscreen buttons to toggle fulllscreen on/off.
  void enterFullscreen() override;
  void exitFullscreen() override;
  void showOverlayCastButtonIfNeeded() override;
  void toggleOverflowMenu() override;
  bool overflowMenuVisible() override;
  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the `MediaControlsEnabled` setting has changed.
  void onMediaControlsEnabledChange() override {
    // There is no update because only the overlay is expected to change.
    refreshCastButtonVisibilityWithoutUpdate();
  }
  // Methods called by MediaControlsMediaEventListener.
  void onVolumeChange() override;
  void onFocusIn() override;
  void onTimeUpdate() override;
  void onDurationChange() override;
  void onPlay() override;
  void onPlaying() override;
  void onPause() override;
  void onTextTracksAddedOrRemoved() override;
  void onTextTracksChanged() override;
  void onError() override;
  void onLoadedMetadata() override;
  void onEnteredFullscreen() override;
  void onExitedFullscreen() override;
  Document& ownerDocument() { return document(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaControlsMediaEventListener;
  friend class MediaControlsOrientationLockDelegateTest;
  friend class MediaControlsImplTest;

  // Need to be members of MediaControls for private member access.
  class BatchedControlUpdate;
  class MediaControlsResizeObserverCallback;

  static MediaControlsImpl* create(HTMLMediaElement&, ShadowRoot&);

  void invalidate(Element*);

  // Notify us that our controls enclosure has changed size.
  void notifyElementSizeChanged(ClientRect* newSize);

  explicit MediaControlsImpl(HTMLMediaElement&);

  void initializeControls();

  void makeOpaque();
  void makeTransparent();
  bool isVisible() const;

  void updatePlayState();

  enum HideBehaviorFlags {
    IgnoreNone = 0,
    IgnoreVideoHover = 1 << 0,
    IgnoreFocus = 1 << 1,
    IgnoreControlsHover = 1 << 2,
    IgnoreWaitForTimer = 1 << 3,
  };

  bool shouldHideMediaControls(unsigned behaviorFlags = 0) const;
  void hideMediaControlsTimerFired(TimerBase*);
  void startHideMediaControlsTimer();
  void stopHideMediaControlsTimer();
  void resetHideMediaControlsTimer();

  void elementSizeChangedTimerFired(TimerBase*);

  void hideAllMenus();

  // Hide elements that don't fit, and show those things that we want which
  // do fit.  This requires that m_effectiveWidth and m_effectiveHeight are
  // current.
  void computeWhichControlsFit();

  // Node
  bool isMediaControls() const override { return true; }
  bool willRespondToMouseMoveEvents() override { return true; }
  void defaultEventHandler(Event*) override;
  bool containsRelatedTarget(Event*);

  // Internal cast related methods.
  void startedCasting();
  void stoppedCasting();
  void refreshCastButtonVisibility();
  void refreshCastButtonVisibilityWithoutUpdate();

  // Media control elements.
  Member<MediaControlOverlayEnclosureElement> m_overlayEnclosure;
  Member<MediaControlOverlayPlayButtonElement> m_overlayPlayButton;
  Member<MediaControlCastButtonElement> m_overlayCastButton;
  Member<MediaControlPanelEnclosureElement> m_enclosure;
  Member<MediaControlPanelElement> m_panel;
  Member<MediaControlPlayButtonElement> m_playButton;
  Member<MediaControlTimelineElement> m_timeline;
  Member<MediaControlCurrentTimeDisplayElement> m_currentTimeDisplay;
  Member<MediaControlTimeRemainingDisplayElement> m_durationDisplay;
  Member<MediaControlMuteButtonElement> m_muteButton;
  Member<MediaControlVolumeSliderElement> m_volumeSlider;
  Member<MediaControlToggleClosedCaptionsButtonElement>
      m_toggleClosedCaptionsButton;
  Member<MediaControlTextTrackListElement> m_textTrackList;
  Member<MediaControlOverflowMenuButtonElement> m_overflowMenu;
  Member<MediaControlOverflowMenuListElement> m_overflowList;

  Member<MediaControlCastButtonElement> m_castButton;
  Member<MediaControlFullscreenButtonElement> m_fullscreenButton;
  Member<MediaControlDownloadButtonElement> m_downloadButton;

  Member<MediaControlsMediaEventListener> m_mediaEventListener;
  Member<MediaControlsWindowEventListener> m_windowEventListener;
  Member<MediaControlsOrientationLockDelegate> m_orientationLockDelegate;

  TaskRunnerTimer<MediaControlsImpl> m_hideMediaControlsTimer;
  unsigned m_hideTimerBehaviorFlags;
  bool m_isMouseOverControls : 1;
  bool m_isPausedForScrubbing : 1;

  // Watches the video element for resize and updates media controls as
  // necessary.
  Member<ResizeObserver> m_resizeObserver;

  TaskRunnerTimer<MediaControlsImpl> m_elementSizeChangedTimer;
  IntSize m_size;

  bool m_keepShowingUntilTimerFires : 1;
};

DEFINE_ELEMENT_TYPE_CASTS(MediaControlsImpl, isMediaControls());

}  // namespace blink

#endif
