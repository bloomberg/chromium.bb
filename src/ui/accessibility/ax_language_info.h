// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LANGUAGE_INFO_H_
#define UI_ACCESSIBILITY_AX_LANGUAGE_INFO_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXNode;

// An instance of LanguageInfo is used to record any specified language
// attributes and will eventually be extended to include required information
// for language detection.
class AX_EXPORT AXLanguageInfo {
 public:
  AXLanguageInfo(const AXNode* node, std::string lang)
      : node_(node), language_(lang) {}

  AXLanguageInfo(const AXLanguageInfo& info, const AXNode* node)
      : node_(node), language_(info.language_) {}

  AXLanguageInfo(const AXLanguageInfo* info, const AXNode* node)
      : node_(node), language_(info->language_) {}

  ~AXLanguageInfo() {}

  const AXNode* node() const { return node_; }

  // Get language code in BCP 47.
  const std::string& language() const { return language_; }

 private:
  const AXNode* const node_;
  // Language code in BCP 47.
  const std::string language_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AXLanguageInfo);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LANGUAGE_INFO
