// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_resource_loader.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/resources/grit/media_controls_touchless_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

MediaControlsTouchlessResourceLoader::MediaControlsTouchlessResourceLoader()
    : UAStyleSheetLoader() {}

MediaControlsTouchlessResourceLoader::~MediaControlsTouchlessResourceLoader() =
    default;

String MediaControlsTouchlessResourceLoader::GetMediaControlsTouchlessCSS()
    const {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_TOUCHLESS_CSS);
}

String MediaControlsTouchlessResourceLoader::GetUAStyleSheet() {
  return GetMediaControlsTouchlessCSS();
}

void MediaControlsTouchlessResourceLoader::
    InjectMediaControlsTouchlessUAStyleSheet() {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  std::unique_ptr<MediaControlsTouchlessResourceLoader> loader =
      std::make_unique<MediaControlsTouchlessResourceLoader>();

  if (!default_style_sheets.HasMediaControlsStyleSheetLoader())
    default_style_sheets.SetMediaControlsStyleSheetLoader(std::move(loader));
}

}  // namespace blink
