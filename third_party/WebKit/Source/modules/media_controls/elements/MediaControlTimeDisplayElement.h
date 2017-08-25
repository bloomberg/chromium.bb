// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlTimeDisplayElement_h
#define MediaControlTimeDisplayElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class MediaControlsImpl;

class MediaControlTimeDisplayElement : public MediaControlDivElement {
 public:
  // Exported to be used in unit tests.
  MODULES_EXPORT void SetCurrentValue(double);
  // Exported to be used by modules/accessibility.
  MODULES_EXPORT double CurrentValue() const;

 protected:
  MediaControlTimeDisplayElement(MediaControlsImpl&, MediaControlElementType);

  virtual String FormatTime() const;

 private:
  double current_value_ = 0;
};

}  // namespace blink

#endif  // MediaControlTimeDisplayElement_h
