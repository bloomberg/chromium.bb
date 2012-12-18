// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_animation_id_provider.h"

namespace webkit {

int WebAnimationIdProvider::NextAnimationId() {
  static int next_animation_id = 1;
  return next_animation_id++;
}

int WebAnimationIdProvider::NextGroupId() {
  static int next_group_id = 1;
  return next_group_id++;
}

}  // namespace webkit
