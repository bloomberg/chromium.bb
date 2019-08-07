// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_JSON_CONVERTER_H_
#define UI_DISPLAY_MANAGER_JSON_CONVERTER_H_

#include "ui/display/manager/display_manager_export.h"

namespace base {
class Value;
}

namespace display {
class DisplayLayout;

DISPLAY_MANAGER_EXPORT bool JsonToDisplayLayout(const base::Value& value,
                                                DisplayLayout* layout);

DISPLAY_MANAGER_EXPORT bool DisplayLayoutToJson(const DisplayLayout& layout,
                                                base::Value* value);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_JSON_CONVERTER_H_
