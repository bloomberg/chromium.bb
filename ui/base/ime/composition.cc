// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/composition.h"

namespace ui {

Composition::Composition() {
}

Composition::~Composition() {
}

void Composition::Clear() {
  text.clear();
  underlines.clear();
  selection = Range();
}

}  // namespace ui
