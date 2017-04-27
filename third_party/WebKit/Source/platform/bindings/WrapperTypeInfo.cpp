// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/WrapperTypeInfo.h"

namespace blink {

static_assert(offsetof(struct WrapperTypeInfo, gin_embedder) ==
                  offsetof(struct gin::WrapperInfo, embedder),
              "offset of WrapperTypeInfo.ginEmbedder must be the same as "
              "gin::WrapperInfo.embedder");

}  // namespace blink
