// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnection_h
#define WebPresentationConnection_h

#include "public/platform/WebCommon.h"

namespace blink {

// Implemented by the embedder for a PresentationConnection owned by a
// controlling frame.
class WebPresentationConnection {
 public:
  virtual ~WebPresentationConnection() = default;

  // Initializes a controller PresentationConnection.
  virtual void Init() = 0;
};

}  // namespace blink

#endif  // WebPresentationConnection_h
