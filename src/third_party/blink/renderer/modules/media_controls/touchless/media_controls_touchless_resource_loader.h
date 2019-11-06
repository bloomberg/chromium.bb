// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_RESOURCE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_RESOURCE_LOADER_H_

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"

namespace blink {

// Builds the UA stylesheet for the touchless Media Controls.
class MediaControlsTouchlessResourceLoader
    : public CSSDefaultStyleSheets::UAStyleSheetLoader {
 public:
  static void InjectMediaControlsTouchlessUAStyleSheet();

  String GetUAStyleSheet() override;
  MediaControlsTouchlessResourceLoader();
  ~MediaControlsTouchlessResourceLoader() override;

 private:
  String GetMediaControlsTouchlessCSS() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_RESOURCE_LOADER_H_
