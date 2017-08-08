// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerSettings_h
#define WorkerSettings_h

#include "core/CoreExport.h"
#include "core/frame/Settings.h"
#include "platform/fonts/GenericFontFamilySettings.h"

namespace blink {

class CORE_EXPORT WorkerSettings {
 public:
  explicit WorkerSettings(Settings*);

  bool DisableReadingFromCanvas() const { return disable_reading_from_canvas_; }
  bool GetStrictMixedContentChecking() const {
    return strict_mixed_content_checking_;
  }
  bool GetAllowRunningOfInsecureContent() const {
    return allow_running_of_insecure_content_;
  }
  bool GetStrictlyBlockBlockableMixedContent() const {
    return strictly_block_blockable_mixed_content_;
  }

  const GenericFontFamilySettings& GetGenericFontFamilySettings() const {
    return generic_font_family_settings_;
  }

  void MakeGenericFontFamilySettingsAtomic() {
    generic_font_family_settings_.MakeAtomic();
  }

 private:
  void CopyFlagValuesFromSettings(Settings*);

  // The settings that are to be copied from main thread to worker thread
  // These setting's flag values must remain unchanged throughout the document
  // lifecycle.
  // We hard-code the flags as there're very few flags at this moment.
  bool disable_reading_from_canvas_ = false;
  bool strict_mixed_content_checking_ = false;
  bool allow_running_of_insecure_content_ = false;
  bool strictly_block_blockable_mixed_content_ = false;

  GenericFontFamilySettings generic_font_family_settings_;
};

}  // namespace blink

#endif  // WorkerSettings_h
