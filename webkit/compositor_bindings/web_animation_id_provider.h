// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_ANIMATION_ID_PROVIDER
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_ANIMATION_ID_PROVIDER

namespace webkit {

class WebAnimationIdProvider {
 public:
  // These functions each return monotonically increasing values.
  static int NextAnimationId();
  static int NextGroupId();
};

}

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_ANIMATION_ID_PROVIDER
