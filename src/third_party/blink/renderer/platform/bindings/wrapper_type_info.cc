// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

#include "third_party/blink/renderer/platform/bindings/custom_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

namespace blink {

static_assert(offsetof(struct WrapperTypeInfo, gin_embedder) ==
                  offsetof(struct gin::WrapperInfo, embedder),
              "offset of WrapperTypeInfo.ginEmbedder must be the same as "
              "gin::WrapperInfo.embedder");

void WrapperTypeInfo::Trace(Visitor* visitor, const void* impl) const {
  switch (wrapper_class_id) {
    case WrapperTypeInfo::kNodeClassId:
    case WrapperTypeInfo::kObjectClassId:
      visitor->Trace(reinterpret_cast<const ScriptWrappable*>(impl));
      break;
    case WrapperTypeInfo::kCustomWrappableId:
      visitor->Trace(reinterpret_cast<const CustomWrappable*>(impl));
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
