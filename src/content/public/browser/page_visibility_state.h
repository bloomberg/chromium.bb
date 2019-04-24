// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_VISIBILITY_STATE_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_VISIBILITY_STATE_H_

namespace content {

enum class PageVisibilityState {
  kVisible,
  kHidden,
  kPrerender,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_VISIBILITY_STATE_H_
