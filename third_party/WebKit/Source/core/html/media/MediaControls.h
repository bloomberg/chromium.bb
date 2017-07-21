// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControls_h
#define MediaControls_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"

namespace blink {

class HTMLDivElement;
class HTMLMediaElement;
class LayoutObject;

// MediaControls is an interface to abstract the HTMLMediaElement controls. The
// implementation will be used using a Factory (see below).
class CORE_EXPORT MediaControls : public GarbageCollectedMixin {
 public:
  MediaControls(HTMLMediaElement&);
  virtual ~MediaControls() = default;

  HTMLMediaElement& MediaElement() const;

  // Enables showing of the controls - only shows if appropriate.
  virtual void MaybeShow() = 0;
  // Disables showing of the controls - immediately hides.
  virtual void Hide() = 0;

  virtual void Reset() = 0;

  // Notify the media controls that the controlsList attribute has changed.
  virtual void OnControlsListUpdated() = 0;

  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  virtual void OnTrackElementFailedToLoad() = 0;

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
  virtual HTMLDivElement* PanelElement() = 0;
  virtual void OnMediaControlsEnabledChange() = 0;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<HTMLMediaElement> media_element_;
};

}  // namespace blink

#endif  // MediaControls_h
