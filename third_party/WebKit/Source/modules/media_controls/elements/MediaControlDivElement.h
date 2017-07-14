// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlDivElement_h
#define MediaControlDivElement_h

#include "core/html/HTMLDivElement.h"
#include "modules/media_controls/elements/MediaControlElementBase.h"

namespace blink {

class MediaControlsImpl;

// MediaControlElementBase implementation based on a <div>. Used for panels, and
// floating UI.
class MediaControlDivElement : public HTMLDivElement,
                               public MediaControlElementBase {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlDivElement);

  // Implements MediaControlElementBase.
  void SetOverflowElementIsWanted(bool) final;
  void MaybeRecordDisplayed() final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlDivElement(MediaControlsImpl&, MediaControlElementType);

 private:
  bool IsMediaControlElement() const final;
};

}  // namespace blink

#endif  // MediaControlDivElement_h
