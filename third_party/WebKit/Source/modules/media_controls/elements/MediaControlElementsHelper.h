// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlElementsHelper_h
#define MediaControlElementsHelper_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlElementType.h"
#include "platform/wtf/Allocator.h"

namespace WTF {
class AtomicString;
}  // namespace WTF

namespace blink {

class ContainerNode;
class Event;
class HTMLDivElement;
class HTMLMediaElement;
class LayoutObject;
class Node;

// Helper class for media control elements. It contains methods, constants or
// concepts shared by more than one element.
class MediaControlElementsHelper final {
  STATIC_ONLY(MediaControlElementsHelper);

 public:
  static bool IsUserInteractionEvent(Event*);

  // Sliders (the volume control and timeline) need to capture some additional
  // events used when dragging the thumb.
  static bool IsUserInteractionEventForSlider(Event*, LayoutObject*);

  // Returns the MediaControlElementType associated with a given |Node|. The
  // |node| _must_ be a media control element.
  // Exported to be used by the accessibility module.
  MODULES_EXPORT static MediaControlElementType GetMediaControlElementType(
      const Node*);

  // Returns the media element associated with a given |node|.
  // Exported to be used by the accessibility module.
  MODULES_EXPORT static const HTMLMediaElement* ToParentMediaElement(
      const Node*);

  // Utility function for quickly creating div elements.
  static HTMLDivElement* CreateDiv(const WTF::AtomicString& id,
                                   ContainerNode* parent);
};

}  // namespace blink

#endif  // MediaControlElementsHelper_h
