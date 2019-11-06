// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_FONTCONFIG_MATCHING_H_
#define COMPONENTS_SERVICES_FONT_FONTCONFIG_MATCHING_H_

#include "base/files/file_path.h"
#include "base/optional.h"

namespace font_service {
// Searches FontConfig for a system font uniquely identified by full font name
// or postscript name. The matching algorithm tries to match both. Used for
// matching @font-face { src: local() } references in Blink.
class FontConfigLocalMatching {
 public:
  struct FontConfigMatchResult {
    base::FilePath file_path;
    unsigned ttc_index;
  };

  static base::Optional<FontConfigMatchResult>
  FindFontByPostscriptNameOrFullFontName(const std::string& font_name);

 private:
  static base::Optional<FontConfigMatchResult> FindFontBySpecifiedName(
      const char* fontconfig_parameter_name,
      const std::string& font_name);
};

}  // namespace font_service

#endif
