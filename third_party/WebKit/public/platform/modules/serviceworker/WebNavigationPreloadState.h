// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNavigationPreloadState_h
#define WebNavigationPreloadState_h

namespace blink {

struct WebNavigationPreloadState {
  WebNavigationPreloadState(bool enabled, const WebString& header_value)
      : enabled(enabled), header_value(header_value) {}

  bool enabled;
  WebString header_value;
};
}

#endif  // WebNavigationPreloadState_h
