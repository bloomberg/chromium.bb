// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControls_h
#define MediaControls_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"

namespace blink {

class Document;
class HTMLMediaElement;
class LayoutObject;
class MediaControlCastButtonElement;
class MediaControlPanelElement;
class MediaControlTimelineElement;
class MediaControlVolumeSliderElement;
class ShadowRoot;

// MediaControls is an interface to abstract the HTMLMediaElement controls. The
// implementation will be used using a Factory (see below).
class CORE_EXPORT MediaControls : public GarbageCollectedMixin {
 public:
  // Factory class that HTMLMediaElement uses to create the MediaControls
  // instance. MediaControls implementations are expected to implement a factory
  // class and provide an implementation of it to HTMLMediaElement via
  // `::registerMediaControlsFactory()`.
  class Factory {
   public:
    virtual MediaControls* Create(HTMLMediaElement&, ShadowRoot&) = 0;
  };

  MediaControls(HTMLMediaElement&);
  virtual ~MediaControls() = default;

  HTMLMediaElement& MediaElement() const;

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Reset() = 0;

  // Notify the media controls that the controlsList attribute has changed.
  virtual void OnControlsListUpdated() = 0;

  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  virtual void OnTrackElementFailedToLoad() = 0;

  // TODO(mlamouri): the following methods will be able to become private when
  // the controls have moved to modules/ and have access to RemotePlayback.
  virtual void OnRemotePlaybackAvailabilityChanged() = 0;
  virtual void OnRemotePlaybackConnecting() = 0;
  virtual void OnRemotePlaybackDisconnected() = 0;

  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the attribute have changed.
  virtual void OnDisableRemotePlaybackAttributeChanged() = 0;

  // TODO(mlamouri): this method should be moved away from the interface to
  // become an implementation detail.
  virtual void NetworkStateChanged() = 0;

  // Returns the layout object for the part of the controls that should be
  // used for overlap checking during text track layout. May be null.
  // TODO(mlamouri): required by LayoutVTTCue.
  virtual LayoutObject* PanelLayoutObject() = 0;
  // Returns the layout object of the media controls container. Maybe null.
  // TODO(mlamouri): required by LayoutVTTCue.
  virtual LayoutObject* ContainerLayoutObject() = 0;

  // TODO: the following are required by other parts of the media controls
  // implementation and could be removed when the full implementation has moved
  // to modules.
  virtual MediaControlPanelElement* PanelElement() = 0;
  virtual MediaControlTimelineElement* TimelineElement() = 0;
  virtual MediaControlCastButtonElement* CastButtonElement() = 0;
  virtual MediaControlVolumeSliderElement* VolumeSliderElement() = 0;
  virtual Document& OwnerDocument() = 0;
  virtual void OnVolumeChange() = 0;
  virtual void OnFocusIn() = 0;
  virtual void OnTimeUpdate() = 0;
  virtual void OnDurationChange() = 0;
  virtual void OnPlay() = 0;
  virtual void OnPlaying() = 0;
  virtual void OnPause() = 0;
  virtual void OnTextTracksAddedOrRemoved() = 0;
  virtual void OnTextTracksChanged() = 0;
  virtual void OnError() = 0;
  virtual void OnLoadedMetadata() = 0;
  virtual void OnEnteredFullscreen() = 0;
  virtual void OnExitedFullscreen() = 0;
  virtual void OnPanelKeypress() = 0;
  virtual void BeginScrubbing() = 0;
  virtual void EndScrubbing() = 0;
  virtual void UpdateCurrentTimeDisplay() = 0;
  virtual void ToggleTextTrackList() = 0;
  virtual void ShowTextTrackAtIndex(unsigned index_to_enable) = 0;
  virtual void DisableShowingTextTracks() = 0;
  virtual void EnterFullscreen() = 0;
  virtual void ExitFullscreen() = 0;
  virtual void ShowOverlayCastButtonIfNeeded() = 0;
  virtual void ToggleOverflowMenu() = 0;
  virtual bool OverflowMenuVisible() = 0;
  virtual void OnMediaControlsEnabledChange() = 0;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<HTMLMediaElement> media_element_;
};

}  // namespace blink

#endif  // MediaControls_h
