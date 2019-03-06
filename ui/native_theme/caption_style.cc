// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"

namespace ui {

CaptionStyle::CaptionStyle() = default;
CaptionStyle::CaptionStyle(const CaptionStyle& other) = default;
CaptionStyle::~CaptionStyle() = default;

// static
CaptionStyle CaptionStyle::FromSpec(const std::string& spec) {
  CaptionStyle style;
  std::unique_ptr<base::Value> dict = base::JSONReader::ReadDeprecated(spec);

  if (!dict || !dict->is_dict())
    return style;

  if (const std::string* value = dict->FindStringKey("text-color"))
    style.text_color = *value;
  if (const std::string* value = dict->FindStringKey("background-color"))
    style.background_color = *value;

  return style;
}

#if !defined(OS_WIN)
CaptionStyle CaptionStyle::FromSystemSettings() {
  return CaptionStyle();
}
#endif

}  // namespace ui
