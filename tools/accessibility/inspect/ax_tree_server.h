// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AX_TREE_SERVER_H_
#define AX_TREE_SERVER_H_

#include <string>

#include "content/browser/accessibility/accessibility_tree_formatter.h"

namespace content {

class AXTreeServer {
 public:
  AXTreeServer(base::ProcessId pid, base::string16& filters_path);
  AXTreeServer(gfx::AcceleratedWidget widget, base::string16& filters_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeServer);

  void Format(AccessibilityTreeFormatter& formatter,
              base::DictionaryValue& dict,
              base::string16& filters_path);
};

}  // namespace content

#endif  // AX_TREE_SERVER_H_
