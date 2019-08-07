// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"

#include "base/logging.h"

ContentSettingImageModelStates::~ContentSettingImageModelStates() = default;

// static
ContentSettingImageModelStates* ContentSettingImageModelStates::Get(
    content::WebContents* contents) {
  DCHECK(contents);
  if (auto* state = FromWebContents(contents))
    return state;
  CreateForWebContents(contents);
  return FromWebContents(contents);
}

void ContentSettingImageModelStates::SetAnimationHasRun(
    ImageType type,
    bool animation_has_run) {
  VerifyType(type);
  animations_[static_cast<int>(type)] = animation_has_run;
}

bool ContentSettingImageModelStates::AnimationHasRun(ImageType type) const {
  VerifyType(type);
  return animations_[static_cast<int>(type)];
}

ContentSettingImageModelStates::ContentSettingImageModelStates(
    content::WebContents* contents) {}

void ContentSettingImageModelStates::VerifyType(ImageType type) const {
  CHECK_GE(type, static_cast<ImageType>(0));
  CHECK_LT(type, ImageType::NUM_IMAGE_TYPES);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentSettingImageModelStates)
