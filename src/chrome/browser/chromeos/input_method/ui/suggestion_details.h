// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_

#include "base/strings/string16.h"

namespace ui {
namespace ime {

struct SuggestionDetails {
  base::string16 text;
  size_t confirmed_length = 0;
  bool show_annotation = false;
  bool show_setting_link = false;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
