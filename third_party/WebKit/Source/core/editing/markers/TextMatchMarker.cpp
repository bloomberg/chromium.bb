// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/TextMatchMarker.h"

namespace blink {

DocumentMarker::MarkerType TextMatchMarker::GetType() const {
  return DocumentMarker::kTextMatch;
}

}  // namespace blink
