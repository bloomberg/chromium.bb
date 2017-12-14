// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkCloneableMessage_h
#define BlinkCloneableMessage_h

#include "base/macros.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/CoreExport.h"

namespace blink {

// This struct represents messages as they are posted over a broadcast channel.
// This type can be serialized as a blink::mojom::CloneableMessage struct.
// This is the renderer-side equivalent of blink::MessagePortMessage, where this
// struct uses blink types, while the other struct uses std:: types.
struct CORE_EXPORT BlinkCloneableMessage {
  BlinkCloneableMessage();
  ~BlinkCloneableMessage();

  BlinkCloneableMessage(BlinkCloneableMessage&&);
  BlinkCloneableMessage& operator=(BlinkCloneableMessage&&);

  scoped_refptr<blink::SerializedScriptValue> message;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkCloneableMessage);
};

}  // namespace blink

#endif  // BlinkCloneableMessage_h
