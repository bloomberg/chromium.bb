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
    virtual MediaControls* create(HTMLMediaElement&, ShadowRoot&) = 0;
  };

  MediaControls(HTMLMediaElement&);
  virtual ~MediaControls() = default;

  HTMLMediaElement& mediaElement() const;

  virtual void show() = 0;
  virtual void hide() = 0;
  virtual void reset() = 0;

  // Notify the media controls that the controlsList attribute has changed.
  virtual void onControlsListUpdated() = 0;

  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  virtual void onTrackElementFailedToLoad() = 0;

  // TODO(mlamouri): the following methods will be able to become private when
  // the controls have moved to modules/ and have access to RemotePlayback.
  virtual void onRemotePlaybackAvailabilityChanged() = 0;
  virtual void onRemotePlaybackConnecting() = 0;
  virtual void onRemotePlaybackDisconnected() = 0;

  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the attribute have changed.
  virtual void onDisableRemotePlaybackAttributeChanged() = 0;

  // TODO(mlamouri): this method should be moved away from the interface to
  // become an implementation detail.
  virtual void networkStateChanged() = 0;

  // Returns the layout object for the part of the controls that should be
  // used for overlap checking during text track layout. May be null.
  // TODO(mlamouri): required by LayoutVTTCue.
  virtual LayoutObject* panelLayoutObject() = 0;
  // Returns the layout object of the media controls container. Maybe null.
  // TODO(mlamouri): required by LayoutVTTCue.
  virtual LayoutObject* containerLayoutObject() = 0;

  // TODO: the following are required by other parts of the media controls
  // implementation and could be removed when the full implementation has moved
  // to modules.
  virtual MediaControlPanelElement* panelElement() = 0;
  virtual MediaControlTimelineElement* timelineElement() = 0;
  virtual MediaControlCastButtonElement* castButtonElement() = 0;
  virtual MediaControlVolumeSliderElement* volumeSliderElement() = 0;
  virtual Document& ownerDocument() = 0;
  virtual void onVolumeChange() = 0;
  virtual void onFocusIn() = 0;
  virtual void onTimeUpdate() = 0;
  virtual void onDurationChange() = 0;
  virtual void onPlay() = 0;
  virtual void onPause() = 0;
  virtual void onTextTracksAddedOrRemoved() = 0;
  virtual void onTextTracksChanged() = 0;
  virtual void onError() = 0;
  virtual void onLoadedMetadata() = 0;
  virtual void onEnteredFullscreen() = 0;
  virtual void onExitedFullscreen() = 0;
  virtual void beginScrubbing() = 0;
  virtual void endScrubbing() = 0;
  virtual void updateCurrentTimeDisplay() = 0;
  virtual void toggleTextTrackList() = 0;
  virtual void showTextTrackAtIndex(unsigned indexToEnable) = 0;
  virtual void disableShowingTextTracks() = 0;
  virtual void enterFullscreen() = 0;
  virtual void exitFullscreen() = 0;
  virtual void showOverlayCastButtonIfNeeded() = 0;
  virtual void toggleOverflowMenu() = 0;
  virtual bool overflowMenuVisible() = 0;
  virtual void onMediaControlsEnabledChange() = 0;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<HTMLMediaElement> m_mediaElement;
};

}  // namespace blink

#endif  // MediaControls_h
