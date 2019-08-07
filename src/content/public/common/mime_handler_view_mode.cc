// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mime_handler_view_mode.h"

#include "content/public/common/content_features.h"

namespace content {

// static
bool MimeHandlerViewMode::UsesCrossProcessFrame() {
  return base::FeatureList::IsEnabled(
      features::kMimeHandlerViewInCrossProcessFrame);
}

}  // namespace content
