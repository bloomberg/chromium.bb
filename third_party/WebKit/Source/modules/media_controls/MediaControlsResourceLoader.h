// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsResourceLoader_h
#define MediaControlsResourceLoader_h

#include "core/css/CSSDefaultStyleSheets.h"

namespace blink {

// Builds the UA stylesheet for the Media Controls based on feature flags
// and platform. There is an Android specific stylesheet that we will include
// on Android or if devtools has enable mobile emulation.
class MediaControlsResourceLoader
    : public CSSDefaultStyleSheets::UAStyleSheetLoader {
 public:
  static void InjectMediaControlsUAStyleSheet();

  // The timeline specific stylesheet is inserted into the timeline DOM tree
  // and contains loading animations specific to the timeline.
  static String GetShadowTimelineStyleSheet();

  // The timeline specific stylesheet is inserted into the loading panel DOM
  // tree and contains styles specific to the loading panel.
  static String GetShadowLoadingStyleSheet();

  String GetUAStyleSheet() override;

  MediaControlsResourceLoader();
  ~MediaControlsResourceLoader() override;

 private:
  String GetMediaControlsCSS() const;

  String GetMediaControlsAndroidCSS() const;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsResourceLoader);
};

}  // namespace blink

#endif  // MediaControlsResourceLoader_h
